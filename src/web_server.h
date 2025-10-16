#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config.h"

// Forward declarations of external variables and functions
extern AsyncWebServer webServer;
extern String hostnameStr;

// External configuration and state structures
extern CoffeeConfig coffeeConfig;
extern SystemState systemState;

// External functions from other modules
#include "storage.h"
#include "temperature.h"
#include "pid_control.h"

// Web server setup function
void setupWebServer();

#endif // WEB_SERVER_H

