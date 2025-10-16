#include <Arduino.h>

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SPI.h>
#include "Adafruit_MAX31855.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <PID_v1.h>
#include <sTune.h>


/* platform.ini
upload_speed = 115200
monitor_speed = 115200
*/
// ======= WiFi and MQTT Settings =======
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// ======= InfluxDB Settings =======
WiFiUDP udp; // UDP for InfluxDB
byte udp_host[] = {192, 168, 10, 7}; // InfluxDB host IP address
int udp_port = 8089; // InfluxDB UDP plugin port


// Set your desired mDNS hostname here
const char* hostname = "coffee";  // will resolve to coffee.local

// ======= MAX31855 K-Type Thermocouple Settings =======
// Define the pins for the MAX31855 breakout board
#define MAX31855_CS   5   // Chip Select pin
#define MAX31855_CLK  18  // Clock pin  
#define MAX31855_DO   19  // Data Out pin

// ======= Heating Element Control =======
#define HEATING_ELEMENT_PIN  2  // GPIO2 for SSR control (Fotek SSR-40 DC)

// Initialize the MAX31855 sensor
Adafruit_MAX31855 thermocouple(MAX31855_CLK, MAX31855_CS, MAX31855_DO);

WiFiClient espClient;

// Hostname and payload for InfluxDB (will be updated from ArduinoOTA)
String hostnameStr = "coffee";
String payload = "default";

// ======= Web Server and Configuration =======
AsyncWebServer webServer(80);
Preferences preferences;

// Coffee Station Configuration Structure
struct CoffeeConfig {
  // Temperature settings (Celsius)
  float brewTemp = 93.0;          // Espresso brewing temperature
  float steamTemp = 150.0;        // Steam wand temperature
  
  // Shot lengths (pump run time in seconds)
  float shotSizes[4] = {15.0, 25.0, 35.0, 45.0};  // Small, Medium, Large, Extra Large
  const char* shotNames[4] = {"Small", "Medium", "Large", "XL"};
  
  // Grind amounts (grinder run time in seconds)
  float grindTimes[2] = {12.0, 18.0};  // Single shot, Double shot
  const char* grindNames[2] = {"Single", "Double"};
  
  // PID parameters
  float pidKp = 2.0;
  float pidKi = 5.0;
  float pidKd = 1.0;
  bool usePID = false;  // false = on/off control, true = PID control
  
  // System settings
  bool enableInfluxDB = true;
  int tempUpdateInterval = 1000;   // milliseconds
  
  // Network settings (for future use)
  char customSSID[32] = "";
  char customPassword[64] = "";
} coffeeConfig;

// Current system state
struct SystemState {
  float currentTemp = 0.0;
  float targetTemp = 0.0;
  bool heatingElement = false;
  bool pump = false;
  bool grinder = false;
  bool steamMode = false;
  String currentOperation = "Idle";
  unsigned long operationStartTime = 0;
} systemState;

// ======= PID Control Variables =======
double pidInput = 0.0;      // Current temperature (input to PID)
double pidOutput = 0.0;     // PID output (0-255, but we'll use 0-100 for percentage)
double pidSetpoint = 0.0;   // Target temperature (setpoint for PID)
PID heatingPID(&pidInput, &pidOutput, &pidSetpoint, 
               coffeeConfig.pidKp, coffeeConfig.pidKi, coffeeConfig.pidKd, DIRECT);

// ======= PID AutoTune Variables =======
float tuneInput = 0.0;
float tuneOutput = 0.0;
sTune tuner = sTune(&tuneInput, &tuneOutput, sTune::ZN_PID, sTune::directIP, sTune::printOFF);
bool autotuning = false;
unsigned long autotuneStartTime = 0;
const unsigned long AUTOTUNE_TIMEOUT = 600000; // 10 minutes timeout

// ======= Global Variables =======
unsigned long previousMillis = 0;
const unsigned long interval = 1000; // 5 seconds

// ======= Forward Declarations =======
void saveConfiguration();

// ====== WiFi connection helper function =======//
bool connectToWiFi(const char* ssid, const char* password, int maxTries = 50) {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < maxTries) {
    delay(500);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected successfully with IP-address:" + String(WiFi.localIP()));
    return true;
  } else {
    Serial.println("\nFailed to connect to WiFi.");
    return false;
  }
}



const int maxMDNSTries = 20;

// ======= Temperature Reading Function =======
float readTemperature() {
  double tempC = thermocouple.readCelsius();
  
  // Check for sensor errors
  if (isnan(tempC)) {
    Serial.println("Error: Thermocouple fault detected!");
    
    // Get specific error information
    uint8_t error = thermocouple.readError();
    if (error & MAX31855_FAULT_OPEN) {
      Serial.println("FAULT: Thermocouple is open - no connections.");
    }
    if (error & MAX31855_FAULT_SHORT_GND) {
      Serial.println("FAULT: Thermocouple is short-circuited to GND.");
    }
    if (error & MAX31855_FAULT_SHORT_VCC) {
      Serial.println("FAULT: Thermocouple is short-circuited to VCC.");
    }
    return -999.0; // Return error value
  }
  
  return tempC;
}

// ======= Heating Element Control Functions =======
void setHeatingElement(bool state) {
  digitalWrite(HEATING_ELEMENT_PIN, state ? HIGH : LOW);
  systemState.heatingElement = state;
  
  Serial.print("Heating element: ");
  Serial.println(state ? "ON" : "OFF");
}

bool getHeatingElement() {
  return systemState.heatingElement;
}

// Temperature control - supports both on/off and PID control
void updateHeatingControl() {
  float currentTemp = systemState.currentTemp;
  float targetTemp = systemState.targetTemp;
  
  if (coffeeConfig.usePID) {
    // PID Control Mode
    pidInput = currentTemp;
    pidSetpoint = targetTemp;
    
    heatingPID.Compute();
    
    // PID output is 0-255, convert to percentage and use threshold
    // For SSR: >50% = ON, <=50% = OFF (you can adjust this threshold)
    float outputPercent = (pidOutput / 255.0) * 100.0;
    
    if (outputPercent > 50.0) {
      if (!systemState.heatingElement) {
        setHeatingElement(true);
      }
    } else {
      if (systemState.heatingElement) {
        setHeatingElement(false);
      }
    }
    
    // Optional: Print PID debug info
    static unsigned long lastDebug = 0;
    if (millis() - lastDebug > 5000) {
      lastDebug = millis();
      Serial.printf("PID: Input=%.2f, Setpoint=%.2f, Output=%.2f (%.1f%%)\n", 
                    pidInput, pidSetpoint, pidOutput, outputPercent);
    }
    
  } else {
    // Simple on/off control with 1°C hysteresis
    if (currentTemp < targetTemp - 1.0) {
      if (!systemState.heatingElement) {
        setHeatingElement(true);
      }
    } else if (currentTemp > targetTemp) {
      if (systemState.heatingElement) {
        setHeatingElement(false);
      }
    }
    // If within 1°C of target, maintain current state
  }
}

// ======= PID AutoTune Functions =======
void startAutotune() {
  if (autotuning) {
    Serial.println("Autotune already running!");
    return;
  }
  
  // Configure autotune for espresso machine
  // Configure(inputSpan, outputSpan, outputStart, outputStep, testTimeSec, settleTimeSec, samples)
  tuner.Configure(50.0,     // Input span (temperature range, e.g., 50°C)
                  255.0,    // Output span (0-255)
                  0.0,      // Output start
                  128.0,    // Output step (50% of range)
                  10,       // Test time (seconds)
                  10,       // Settle time (seconds)
                  300);     // Samples
  
  tuner.SetEmergencyStop(systemState.targetTemp + 10.0); // Emergency stop 10°C above target
  
  autotuning = true;
  autotuneStartTime = millis();
  systemState.currentOperation = "AutoTuning PID";
  
  Serial.println("=== PID AutoTune Started ===");
  Serial.printf("Target Temperature: %.2f°C\n", systemState.targetTemp);
}

void stopAutotune(bool saveResults) {
  if (!autotuning) {
    return;
  }
  
  autotuning = false;
  
  if (saveResults) {
    // Get the tuned parameters
    coffeeConfig.pidKp = tuner.GetKp();
    coffeeConfig.pidKi = tuner.GetKi();
    coffeeConfig.pidKd = tuner.GetKd();
    
    // Update the PID controller with new parameters
    heatingPID.SetTunings(coffeeConfig.pidKp, coffeeConfig.pidKi, coffeeConfig.pidKd);
    
    // Save to flash
    saveConfiguration();
    
    Serial.println("=== AutoTune Complete - Parameters Saved ===");
    Serial.printf("Kp: %.3f, Ki: %.3f, Kd: %.3f\n", 
                  coffeeConfig.pidKp, coffeeConfig.pidKi, coffeeConfig.pidKd);
  } else {
    Serial.println("=== AutoTune Cancelled ===");
  }
  
  systemState.currentOperation = "Idle";
  // Turn off heating element
  setHeatingElement(false);
}

void updateAutotune() {
  if (!autotuning) {
    return;
  }
  
  // Check for timeout
  if (millis() - autotuneStartTime > AUTOTUNE_TIMEOUT) {
    Serial.println("AutoTune timeout - stopping");
    stopAutotune(false);
    return;
  }
  
  // Update autotune with current temperature
  tuneInput = systemState.currentTemp;
  
  // Run the tuner
  switch (tuner.Run()) {
    case tuner.sample:
      // Still sampling, control output based on tuner
      // The output is stored in the tuneOutput variable by reference
      if (tuneOutput > 128) {
        setHeatingElement(true);
      } else {
        setHeatingElement(false);
      }
      break;
      
    case tuner.tunings:
      // Tuning complete
      Serial.println("AutoTune sampling complete!");
      stopAutotune(true);
      break;
      
    case tuner.runPid:
      // Should not happen during autotune
      break;
  }
}

// ======= InfluxDB Send Function =======
void send_value(String location, String value) {
  payload = "temp";
  payload += ",host="     + hostnameStr;
  payload += ",location=" + location;
  payload += " value="    + value;
  udp.beginPacket(udp_host, udp_port);
  udp.print(payload);
  udp.endPacket();
}

// ======= Configuration Management =======
void saveConfiguration() {
  preferences.begin("coffee-config", false);
  
  preferences.putFloat("brewTemp", coffeeConfig.brewTemp);
  preferences.putFloat("steamTemp", coffeeConfig.steamTemp);
  
  // Save shot sizes
  for (int i = 0; i < 4; i++) {
    String key = "shot" + String(i);
    preferences.putFloat(key.c_str(), coffeeConfig.shotSizes[i]);
  }
  
  // Save grind times
  for (int i = 0; i < 2; i++) {
    String key = "grind" + String(i);
    preferences.putFloat(key.c_str(), coffeeConfig.grindTimes[i]);
  }
  
  preferences.putFloat("pidKp", coffeeConfig.pidKp);
  preferences.putFloat("pidKi", coffeeConfig.pidKi);
  preferences.putFloat("pidKd", coffeeConfig.pidKd);
  preferences.putBool("usePID", coffeeConfig.usePID);
  preferences.putBool("influxEnable", coffeeConfig.enableInfluxDB);
  preferences.putInt("tempInterval", coffeeConfig.tempUpdateInterval);
  
  preferences.end();
  Serial.println("Configuration saved to flash memory");
}

void loadConfiguration() {
  preferences.begin("coffee-config", true); // read-only
  
  coffeeConfig.brewTemp = preferences.getFloat("brewTemp", 93.0);
  coffeeConfig.steamTemp = preferences.getFloat("steamTemp", 150.0);
  
  // Load shot sizes
  for (int i = 0; i < 4; i++) {
    String key = "shot" + String(i);
    coffeeConfig.shotSizes[i] = preferences.getFloat(key.c_str(), coffeeConfig.shotSizes[i]);
  }
  
  // Load grind times
  for (int i = 0; i < 2; i++) {
    String key = "grind" + String(i);
    coffeeConfig.grindTimes[i] = preferences.getFloat(key.c_str(), coffeeConfig.grindTimes[i]);
  }
  
  coffeeConfig.pidKp = preferences.getFloat("pidKp", 2.0);
  coffeeConfig.pidKi = preferences.getFloat("pidKi", 5.0);
  coffeeConfig.pidKd = preferences.getFloat("pidKd", 1.0);
  coffeeConfig.usePID = preferences.getBool("usePID", false);
  coffeeConfig.enableInfluxDB = preferences.getBool("influxEnable", true);
  coffeeConfig.tempUpdateInterval = preferences.getInt("tempInterval", 1000);
  
  preferences.end();
  Serial.println("Configuration loaded from flash memory");
}

// ======= Web Server Endpoints =======
void setupWebServer() {
  // Serve main configuration page
  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Coffee Station Control</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }
        .container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }
        .header { text-align: center; color: #8B4513; margin-bottom: 30px; }
        .section { margin: 20px 0; padding: 15px; border: 1px solid #ddd; border-radius: 5px; }
        .status { background-color: #e8f5e8; }
        .config { background-color: #f8f8ff; }
        input[type="number"] { width: 80px; padding: 5px; margin: 5px; }
        button { background-color: #8B4513; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 5px; }
        button:hover { background-color: #A0522D; }
        .status-display { font-size: 18px; font-weight: bold; }
        .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    </style>
</head>
<body>
    <div class="container">
        <h1 class="header">☕ Coffee Station Control</h1>
        
        <div class="section status">
            <h2>Current Status</h2>
            <div id="status" class="status-display">Loading...</div>
            <div style="margin-top: 15px;">
                <button onclick="toggleHeating()">Toggle Heating</button>
                <button onclick="setBrewMode()">Brew Mode</button>
                <button onclick="setSteamMode()">Steam Mode</button>
            </div>
        </div>
        
        <div class="section config">
            <h2>Temperature Settings</h2>
            <div class="grid">
                <div>
                    <label>Brew Temperature (&deg;C):</label><br>
                    <input type="number" id="brewTemp" step="0.5" min="80" max="100">
                </div>
                <div>
                    <label>Steam Temperature (&deg;C):</label><br>
                    <input type="number" id="steamTemp" step="0.5" min="100" max="170">
                </div>
            </div>
        </div>
        
        <div class="section config">
            <h2>Shot Sizes (seconds)</h2>
            <div class="grid">
                <div><label>Small:</label><br><input type="number" id="shot0" step="0.5" min="5" max="60"></div>
                <div><label>Medium:</label><br><input type="number" id="shot1" step="0.5" min="5" max="60"></div>
                <div><label>Large:</label><br><input type="number" id="shot2" step="0.5" min="5" max="60"></div>
                <div><label>Extra Large:</label><br><input type="number" id="shot3" step="0.5" min="5" max="60"></div>
            </div>
        </div>
        
        <div class="section config">
            <h2>Grind Times (seconds)</h2>
            <div class="grid">
                <div>
                    <label>Single Shot:</label><br>
                    <input type="number" id="grind0" step="0.5" min="5" max="30">
                </div>
                <div>
                    <label>Double Shot:</label><br>
                    <input type="number" id="grind1" step="0.5" min="5" max="30">
                </div>
            </div>
        </div>
        
        <div class="section config">
            <h2>PID Control Parameters</h2>
            <div style="margin-bottom: 15px;">
                <label><b>Control Mode:</b></label><br>
                <input type="checkbox" id="usePID"> Use PID Control (unchecked = simple on/off control)
            </div>
            <div class="grid">
                <div>
                    <label>Proportional (Kp):</label><br>
                    <input type="number" id="pidKp" step="0.1" min="0" max="20">
                </div>
                <div>
                    <label>Integral (Ki):</label><br>
                    <input type="number" id="pidKi" step="0.1" min="0" max="20">
                </div>
                <div>
                    <label>Derivative (Kd):</label><br>
                    <input type="number" id="pidKd" step="0.1" min="0" max="10">
                </div>
            </div>
            <div style="margin-top: 15px;">
                <button onclick="startAutotune()" id="autotuneBtn">Start PID AutoTune</button>
                <button onclick="stopAutotune()" id="stopAutotuneBtn" style="display:none; background-color:#c00;">Stop AutoTune</button>
                <span id="autotuneStatus" style="margin-left: 10px; font-weight: bold;"></span>
            </div>
        </div>
        
        <div class="section config">
            <h2>System Settings</h2>
            <label><input type="checkbox" id="enableInflux"> Enable InfluxDB Logging</label><br>
            <label>Temperature Update Interval (ms):</label>
            <input type="number" id="tempInterval" step="100" min="500" max="5000">
        </div>
        
        <div style="text-align: center; margin-top: 20px;">
            <button onclick="loadConfig()">Reload Config</button>
            <button onclick="saveConfig()">Save Configuration</button>
        </div>
    </div>
    
    <script>
        function updateStatus() {
            fetch('/api/status')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('status').innerHTML = `
                        Temperature: ${data.currentTemp}&deg;C (Target: ${data.targetTemp}&deg;C)<br>
                        Operation: ${data.currentOperation}<br>
                        Heating: ${data.heatingElement ? 'ON' : 'OFF'} | 
                        Pump: ${data.pump ? 'ON' : 'OFF'} | 
                        Grinder: ${data.grinder ? 'ON' : 'OFF'}
                    `;
                });
        }
        
        function loadConfig() {
            fetch('/api/config')
                .then(response => response.json())
                .then(config => {
                    document.getElementById('brewTemp').value = config.brewTemp;
                    document.getElementById('steamTemp').value = config.steamTemp;
                    for(let i = 0; i < 4; i++) {
                        document.getElementById('shot' + i).value = config.shotSizes[i];
                    }
                    for(let i = 0; i < 2; i++) {
                        document.getElementById('grind' + i).value = config.grindTimes[i];
                    }
                    document.getElementById('pidKp').value = config.pidKp;
                    document.getElementById('pidKi').value = config.pidKi;
                    document.getElementById('pidKd').value = config.pidKd;
                    document.getElementById('usePID').checked = config.usePID;
                    document.getElementById('enableInflux').checked = config.enableInfluxDB;
                    document.getElementById('tempInterval').value = config.tempUpdateInterval;
                });
        }
        
        function saveConfig() {
            const config = {
                brewTemp: parseFloat(document.getElementById('brewTemp').value),
                steamTemp: parseFloat(document.getElementById('steamTemp').value),
                shotSizes: [],
                grindTimes: [],
                pidKp: parseFloat(document.getElementById('pidKp').value),
                pidKi: parseFloat(document.getElementById('pidKi').value),
                pidKd: parseFloat(document.getElementById('pidKd').value),
                usePID: document.getElementById('usePID').checked,
                enableInfluxDB: document.getElementById('enableInflux').checked,
                tempUpdateInterval: parseInt(document.getElementById('tempInterval').value)
            };
            
            for(let i = 0; i < 4; i++) {
                config.shotSizes[i] = parseFloat(document.getElementById('shot' + i).value);
            }
            for(let i = 0; i < 2; i++) {
                config.grindTimes[i] = parseFloat(document.getElementById('grind' + i).value);
            }
            
            fetch('/api/config', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(config)
            })
            .then(response => response.text())
            .then(data => alert(data));
        }
        
        function toggleHeating() {
            fetch('/api/heating/toggle', {method: 'POST'})
            .then(response => response.text())
            .then(data => {
                console.log(data);
                updateStatus(); // Refresh status immediately
            });
        }
        
        function setBrewMode() {
            fetch('/api/mode/brew', {method: 'POST'})
            .then(response => response.text())
            .then(data => {
                console.log(data);
                updateStatus();
            });
        }
        
        function setSteamMode() {
            fetch('/api/mode/steam', {method: 'POST'})
            .then(response => response.text())
            .then(data => {
                console.log(data);
                updateStatus();
            });
        }
        
        function startAutotune() {
            if (confirm('AutoTune will take several minutes and will cycle the heating element. Continue?')) {
                fetch('/api/autotune/start', {method: 'POST'})
                .then(response => response.text())
                .then(data => {
                    alert(data);
                    updateAutotuneStatus();
                });
            }
        }
        
        function stopAutotune() {
            fetch('/api/autotune/stop', {method: 'POST'})
            .then(response => response.text())
            .then(data => {
                alert(data);
                updateAutotuneStatus();
            });
        }
        
        function updateAutotuneStatus() {
            fetch('/api/autotune/status')
            .then(response => response.json())
            .then(data => {
                const statusSpan = document.getElementById('autotuneStatus');
                const startBtn = document.getElementById('autotuneBtn');
                const stopBtn = document.getElementById('stopAutotuneBtn');
                
                if (data.running) {
                    statusSpan.innerHTML = `Running... (${data.elapsed}s / ${data.timeout}s)`;
                    statusSpan.style.color = '#ff6600';
                    startBtn.style.display = 'none';
                    stopBtn.style.display = 'inline-block';
                } else {
                    statusSpan.innerHTML = '';
                    startBtn.style.display = 'inline-block';
                    stopBtn.style.display = 'none';
                    
                    // Reload config to get updated PID values
                    loadConfig();
                }
            });
        }
        
        // Update status every 2 seconds
        setInterval(updateStatus, 2000);
        setInterval(updateAutotuneStatus, 2000);
        
        // Load initial config
        loadConfig();
        updateStatus();
    </script>
</body>
</html>
    )rawliteral";
    request->send(200, "text/html", html);
  });
  
  // API endpoint: Get current status
  // API endpoint: Get current status
  webServer.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
    JsonDocument doc;
    doc["currentTemp"] = systemState.currentTemp;
    doc["targetTemp"] = systemState.targetTemp;
    doc["heatingElement"] = systemState.heatingElement;
    doc["pump"] = systemState.pump;
    doc["grinder"] = systemState.grinder;
    doc["steamMode"] = systemState.steamMode;
    doc["currentOperation"] = systemState.currentOperation;
    
    String response;
    serializeJson(doc, static_cast<String&>(response));
    request->send(200, "application/json", response);
  });
  
  // API endpoint: Get configuration
  webServer.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request){
    JsonDocument doc;
    doc["brewTemp"] = coffeeConfig.brewTemp;
    doc["steamTemp"] = coffeeConfig.steamTemp;
    
    JsonArray shotSizes = doc.createNestedArray("shotSizes");
    for(int i = 0; i < 4; i++) {
      shotSizes.add(coffeeConfig.shotSizes[i]);
    }
    
    JsonArray grindTimes = doc.createNestedArray("grindTimes");
    for(int i = 0; i < 2; i++) {
      grindTimes.add(coffeeConfig.grindTimes[i]);
    }
    
    doc["pidKp"] = coffeeConfig.pidKp;
    doc["pidKi"] = coffeeConfig.pidKi;
    doc["pidKd"] = coffeeConfig.pidKd;
    doc["usePID"] = coffeeConfig.usePID;
    
    doc["enableInfluxDB"] = coffeeConfig.enableInfluxDB;
    doc["tempUpdateInterval"] = coffeeConfig.tempUpdateInterval;
    
    String response;
    serializeJson(doc, static_cast<String&>(response));
    request->send(200, "application/json", response);
  });
  
  // API endpoint: Update configuration
  webServer.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      JsonDocument doc;
      deserializeJson(doc, (char*)data);
      
      if(doc.containsKey("brewTemp")) coffeeConfig.brewTemp = doc["brewTemp"];
      if(doc.containsKey("steamTemp")) coffeeConfig.steamTemp = doc["steamTemp"];
      
      if(doc.containsKey("shotSizes")) {
        for(int i = 0; i < 4; i++) {
          coffeeConfig.shotSizes[i] = doc["shotSizes"][i];
        }
      }
      
      if(doc.containsKey("grindTimes")) {
        for(int i = 0; i < 2; i++) {
          coffeeConfig.grindTimes[i] = doc["grindTimes"][i];
        }
      }
      
      if(doc.containsKey("pidKp")) coffeeConfig.pidKp = doc["pidKp"];
      if(doc.containsKey("pidKi")) coffeeConfig.pidKi = doc["pidKi"];
      if(doc.containsKey("pidKd")) coffeeConfig.pidKd = doc["pidKd"];
      if(doc.containsKey("usePID")) coffeeConfig.usePID = doc["usePID"];
      
      // Update PID controller with new parameters
      heatingPID.SetTunings(coffeeConfig.pidKp, coffeeConfig.pidKi, coffeeConfig.pidKd);
      
      if(doc.containsKey("enableInfluxDB")) coffeeConfig.enableInfluxDB = doc["enableInfluxDB"];
      if(doc.containsKey("tempUpdateInterval")) coffeeConfig.tempUpdateInterval = doc["tempUpdateInterval"];
      
      saveConfiguration();
      request->send(200, "text/plain", "Configuration saved successfully!");
    }
  );
  
  // API endpoint: Toggle heating element
  webServer.on("/api/heating/toggle", HTTP_POST, [](AsyncWebServerRequest *request){
    bool currentState = getHeatingElement();
    setHeatingElement(!currentState);
    request->send(200, "text/plain", currentState ? "Heating OFF" : "Heating ON");
  });
  
  // API endpoint: Set brew mode
  webServer.on("/api/mode/brew", HTTP_POST, [](AsyncWebServerRequest *request){
    systemState.steamMode = false;
    systemState.targetTemp = coffeeConfig.brewTemp;
    systemState.currentOperation = "Brew Mode";
    request->send(200, "text/plain", "Switched to Brew Mode (" + String(coffeeConfig.brewTemp) + "&deg;C)");
  });
  
  // API endpoint: Set steam mode
  webServer.on("/api/mode/steam", HTTP_POST, [](AsyncWebServerRequest *request){
    systemState.steamMode = true;
    systemState.targetTemp = coffeeConfig.steamTemp;
    systemState.currentOperation = "Steam Mode";
    request->send(200, "text/plain", "Switched to Steam Mode (" + String(coffeeConfig.steamTemp) + "&deg;C)");
  });
  
  // API endpoint: Start PID autotune
  webServer.on("/api/autotune/start", HTTP_POST, [](AsyncWebServerRequest *request){
    if (autotuning) {
      request->send(400, "text/plain", "AutoTune already running!");
    } else {
      startAutotune();
      request->send(200, "text/plain", "AutoTune started - this will take several minutes");
    }
  });
  
  // API endpoint: Stop PID autotune
  webServer.on("/api/autotune/stop", HTTP_POST, [](AsyncWebServerRequest *request){
    if (autotuning) {
      stopAutotune(false);
      request->send(200, "text/plain", "AutoTune cancelled");
    } else {
      request->send(400, "text/plain", "AutoTune not running");
    }
  });
  
  // API endpoint: Get autotune status
  webServer.on("/api/autotune/status", HTTP_GET, [](AsyncWebServerRequest *request){
    JsonDocument doc;
    doc["running"] = autotuning;
    if (autotuning) {
      doc["elapsed"] = (millis() - autotuneStartTime) / 1000; // seconds
      doc["timeout"] = AUTOTUNE_TIMEOUT / 1000; // seconds
    }
    doc["currentKp"] = coffeeConfig.pidKp;
    doc["currentKi"] = coffeeConfig.pidKi;
    doc["currentKd"] = coffeeConfig.pidKd;
    
    String response;
    serializeJson(doc, static_cast<String&>(response));
    request->send(200, "application/json", response);
  });
  
  webServer.begin();
  Serial.println("Web server started on http://" + hostnameStr + ".local/");
}

bool startMDNS() {
  for (int attempt = 1; attempt <= maxMDNSTries; ++attempt) {
    if (MDNS.begin(hostname)) {
      Serial.println("mDNS successful");
      return true;
    } else {
      Serial.print("Attempt ");
      Serial.print(attempt);
      Serial.println(" to set up mDNS failed.");
      delay(500);
    }
  }
  Serial.println("Failed to set up mDNS responder after 20 attempts.");
  return false;
}


// ======= Setup =======
void setup() {
  Serial.begin(115200);
  delay(100);
  
  // Initialize heating element control pin
  pinMode(HEATING_ELEMENT_PIN, OUTPUT);
  digitalWrite(HEATING_ELEMENT_PIN, LOW);  // Start with heating OFF
  Serial.println("Heating element pin initialized (OFF)");

  // WiFi connect
  if (!connectToWiFi(ssid, password)) {
    Serial.println("Failed to connect to WiFi. Check your credentials and network.");
  }

  //Debugging  
  Serial.println(WiFi.RSSI()); // -30 to -70 is good, below -80 is poor
  Serial.println(WiFi.macAddress());
  Serial.println((uint32_t)ESP.getEfuseMac(), HEX);  // ESP.getChipId() is not available on ESP32; use chip MAC address instead
  
  
  // ✅ Set OTA and MDNS hostnames BEFORE begin()
  ArduinoOTA.setHostname(hostname);
 
  // Set up mDNS responder
  if (!startMDNS()) {
    // Handle mDNS failure here, if needed
    Serial.println("Failed to connect mDNS responder");
  }

  MDNS.addService("arduino", "tcp", 3232); // Add Arduino service for OTA
  Serial.print("mDNS responder started: ");
  Serial.println(String(hostname) + ".local");

 
  // Set up OTA
  // This library on ESP32 uses the ESP32 Arduino core's internal OTA API
  // It does NOT accept hostname in .begin()
  ArduinoOTA.begin();  // REQUIRED to enable OTA, hostname comes from MDNS
  ArduinoOTA.onStart([]() {
    Serial.println("OTA Start");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA End");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
  
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();  // REQUIRED to enable OTA, hostname comes from MDNS
  
  // Get hostname for InfluxDB identification
  hostnameStr = ArduinoOTA.getHostname();
  Serial.println("OTA ready. Flash with hostname: " + hostnameStr + ".local");
  Serial.println("InfluxDB will use hostname: " + hostnameStr);
  
  // Load configuration from flash memory
  loadConfiguration();
  
  // Initialize PID controller
  heatingPID.SetMode(AUTOMATIC);
  heatingPID.SetOutputLimits(0, 255);
  heatingPID.SetSampleTime(1000); // 1 second sample time
  Serial.println("PID controller initialized");
  Serial.printf("PID Parameters: Kp=%.3f, Ki=%.3f, Kd=%.3f, Mode=%s\n", 
                coffeeConfig.pidKp, coffeeConfig.pidKi, coffeeConfig.pidKd,
                coffeeConfig.usePID ? "PID" : "On/Off");
  
  // Initialize and start web server
  setupWebServer();

  // Initialize temperature sensor
  Serial.println("Initializing MAX31855 K-type thermocouple sensor...");
  // Test initial reading
  float initialTemp = readTemperature();
  if (initialTemp != -999.0) {
    Serial.print("Initial temperature reading: ");
    Serial.print(initialTemp);
    Serial.println("°C");
  } else {
    Serial.println("Warning: Temperature sensor not detected or faulty");
  }
  
}

// ======= Main Loop =======
void loop() {
  
  ArduinoOTA.handle(); // Required for OTA updates

  
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= coffeeConfig.tempUpdateInterval) {
    previousMillis = currentMillis;
    
    // Read temperature from K-type sensor
    float temperature = readTemperature();
    
    if (temperature != -999.0) {
      // Update system state
      systemState.currentTemp = temperature;
      systemState.targetTemp = systemState.steamMode ? coffeeConfig.steamTemp : coffeeConfig.brewTemp;
      
      Serial.print("Coffee Temperature: ");
      Serial.print(temperature);
      Serial.print("°C (Target: ");
      Serial.print(systemState.targetTemp);
      Serial.println("°C)");
      
      // Send temperature to InfluxDB if enabled
      if (coffeeConfig.enableInfluxDB) {
        send_value("coffee-brew-01", String(temperature));
        send_value("coffe_target-01", String(systemState.targetTemp));
      }
      
      // Update heating control based on temperature
      // If autotuning, use autotune control, otherwise use normal control
      if (autotuning) {
        updateAutotune();
      } else {
        updateHeatingControl();
      }
      
    } else {
      Serial.println("Temperature reading failed - check sensor connection");
      systemState.currentTemp = -999.0;
      // Turn off heating if sensor fails
      if (systemState.heatingElement) {
        setHeatingElement(false);
      }
      // Stop autotune if running
      if (autotuning) {
        stopAutotune(false);
      }
    }
    

  }
  
  
}

// ======= End of main loop =======

