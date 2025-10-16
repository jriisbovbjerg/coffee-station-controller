#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// Coffee Station Modules
#include "config.h"
#include "temperature.h"
#include "pid_control.h"
#include "storage.h"
#include "web_server.h"
#include "credentials.h"  // WiFi and InfluxDB credentials (not in git)

// ======= WiFi Settings =======
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// ======= InfluxDB Settings =======
WiFiUDP udp;
byte udp_host[] = {INFLUXDB_HOST[0], INFLUXDB_HOST[1], INFLUXDB_HOST[2], INFLUXDB_HOST[3]};
int udp_port = INFLUXDB_PORT;

// ======= mDNS Settings =======
const char* hostname = "coffee";
const int maxMDNSTries = 20;

// ======= Global Variables =======
String hostnameStr = "coffee";
String payload = "default";
AsyncWebServer webServer(80);

// Configuration and state instances
CoffeeConfig coffeeConfig;
SystemState systemState;

// Timing variables
unsigned long previousMillis = 0;
bool otaInProgress = false;

// ======= Helper Functions =======
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

void send_value(String location, String value) {
  payload = "temp";
  payload += ",host="     + hostnameStr;
  payload += ",location=" + location;
  payload += " value="    + value;
  udp.beginPacket(udp_host, udp_port);
  udp.print(payload);
  udp.endPacket();
}

// ======= Setup =======
void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n========================================");
  Serial.println("   ESP32 Coffee Station Controller");
  Serial.println("========================================\n");
  
  // Initialize storage and load configuration
  initStorage();
  loadConfiguration();
  
  // Initialize temperature sensor and heating control
  initTemperatureSensor();
  
  // Initialize PID controller
  initPID();
  
  // Connect to WiFi
  if (!connectToWiFi(ssid, password)) {
    Serial.println("Failed to connect to WiFi. Check your credentials and network.");
  }
  
  // Debugging info
  Serial.println("WiFi RSSI: " + String(WiFi.RSSI()));
  Serial.println("MAC Address: " + WiFi.macAddress());
  Serial.println("Chip ID: " + String((uint32_t)ESP.getEfuseMac(), HEX));
  
  // Set up mDNS
  ArduinoOTA.setHostname(hostname);
  if (!startMDNS()) {
    Serial.println("Failed to connect mDNS responder");
  }
  MDNS.addService("arduino", "tcp", 3232);
  Serial.print("mDNS responder started: ");
  Serial.println(String(hostname) + ".local");
  
  // Set up OTA with priority handling
  ArduinoOTA.onStart([]() {
    otaInProgress = true;
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("OTA Start: Updating " + type);
    Serial.println(">>> All normal operations suspended for OTA <<<");
  });

  ArduinoOTA.onEnd([]() {
    otaInProgress = false;
    Serial.println("\n>>> OTA Complete - Rebooting <<<");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    otaInProgress = false;
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  hostnameStr = ArduinoOTA.getHostname();
  Serial.println("OTA ready. Flash with hostname: " + hostnameStr + ".local");
  Serial.println("InfluxDB will use hostname: " + hostnameStr);
  
  // Initialize web server
  setupWebServer();
  
  Serial.println("\n========================================");
  Serial.println("   System Ready");
  Serial.println("========================================\n");
}

// ======= Main Loop =======
void loop() {
  // OTA has highest priority - handle first
  ArduinoOTA.handle();
  
  // Skip all other operations if OTA is in progress
  if (otaInProgress) {
    return;  // Give OTA full CPU time
  }
  
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
      if (isAutotuning()) {
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
      if (isAutotuning()) {
        stopAutotune(false);
      }
    }
  }
}

// ======= End of main loop =======

