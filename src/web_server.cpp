#include "web_server.h"
#include "web_pages.h"

// ======= Web Server Endpoints =======
void setupWebServer() {
  // Serve main configuration page
  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", HTML_PAGE);
  });
  
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
      updatePIDTunings(coffeeConfig.pidKp, coffeeConfig.pidKi, coffeeConfig.pidKd);
      
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
    if (isAutotuning()) {
      request->send(400, "text/plain", "AutoTune already running!");
    } else {
      startAutotune();
      request->send(200, "text/plain", "AutoTune started - this will take several minutes");
    }
  });
  
  // API endpoint: Stop PID autotune
  webServer.on("/api/autotune/stop", HTTP_POST, [](AsyncWebServerRequest *request){
    if (isAutotuning()) {
      stopAutotune(false);
      request->send(200, "text/plain", "AutoTune cancelled");
    } else {
      request->send(400, "text/plain", "AutoTune not running");
    }
  });
  
  // API endpoint: Get autotune status
  webServer.on("/api/autotune/status", HTTP_GET, [](AsyncWebServerRequest *request){
    JsonDocument doc;
    doc["running"] = isAutotuning();
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

