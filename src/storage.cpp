#include "storage.h"
#include <Preferences.h>

static Preferences preferences;

// ======= Storage Initialization =======
void initStorage() {
  Serial.println("Storage system initialized");
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
  coffeeConfig.tempUpdateInterval = preferences.getInt("tempInterval", 2000);
  
  preferences.end();
  Serial.println("Configuration loaded from flash memory");
}

