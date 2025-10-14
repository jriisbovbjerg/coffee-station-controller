#include <Arduino.h>

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SPI.h>
#include "Adafruit_MAX31855.h"


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

// Initialize the MAX31855 sensor
Adafruit_MAX31855 thermocouple(MAX31855_CLK, MAX31855_CS, MAX31855_DO);

WiFiClient espClient;

// Hostname and payload for InfluxDB (will be updated from ArduinoOTA)
String hostnameStr = "coffee";
String payload = "default";

// ======= Global Variables =======
unsigned long previousMillis = 0;
const unsigned long interval = 1000; // 5 seconds

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

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    
    // Read temperature from K-type sensor
    float temperature = readTemperature();
    
    if (temperature != -999.0) {
      Serial.print("Coffee Temperature: ");
      Serial.print(temperature);
      Serial.println("°C");
      
      // Send temperature to InfluxDB
      send_value("coffee-brew-01", String(temperature));
      
    } else {
      Serial.println("Temperature reading failed - check sensor connection");
    }
    

  }
  
  
}

// ======= End of main loop =======

