#include "temperature.h"
#include "Adafruit_MAX31855.h"
#include "pid_control.h"

// ======= MAX31855 K-Type Thermocouple Settings =======
#define MAX31855_CS   5   // Chip Select pin
#define MAX31855_CLK  18  // Clock pin  
#define MAX31855_DO   19  // Data Out pin

// ======= Heating Element Control =======
#define HEATING_ELEMENT_PIN  2  // GPIO2 for SSR control (Fotek SSR-40 DC)

// Initialize the MAX31855 sensor
Adafruit_MAX31855 thermocouple(MAX31855_CLK, MAX31855_CS, MAX31855_DO);

// ======= Temperature Sensor Initialization =======
void initTemperatureSensor() {
  // Initialize heating element control pin
  pinMode(HEATING_ELEMENT_PIN, OUTPUT);
  digitalWrite(HEATING_ELEMENT_PIN, LOW);  // Start with heating OFF
  Serial.println("Heating element pin initialized (OFF)");
  
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

// ======= Temperature Control Function =======
void updateHeatingControl() {
  float currentTemp = systemState.currentTemp;
  float targetTemp = systemState.targetTemp;
  
  if (coffeeConfig.usePID) {
    // PID Control Mode - delegate to PID module
    updatePIDControl(currentTemp, targetTemp);
    
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

