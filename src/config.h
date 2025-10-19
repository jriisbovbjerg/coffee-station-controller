#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

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
  int tempUpdateInterval = 2000;   // milliseconds (2 seconds)
  
  // Network settings (for future use)
  char customSSID[32] = "";
  char customPassword[64] = "";
};

// Current system state
struct SystemState {
  float currentTemp = 0.0;
  float targetTemp = 0.0;
  bool heatingElement = false;
  bool heating = false;  // Display-friendly heating state
  bool pump = false;
  bool grinder = false;
  bool steamMode = false;
  String currentOperation = "Idle";
  unsigned long operationStartTime = 0;
  
  // Display selections
  int selectedShotSize = 0;    // 0-3 for shot sizes
  int selectedGrindTime = 0;   // 0-1 for grind times
};

#endif // CONFIG_H

