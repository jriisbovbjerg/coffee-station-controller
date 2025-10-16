#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <Arduino.h>
#include "config.h"

// External dependencies
extern CoffeeConfig coffeeConfig;
extern SystemState systemState;

// Initialize temperature sensor
void initTemperatureSensor();

// Read temperature from K-type sensor
float readTemperature();

// Heating element control
void setHeatingElement(bool state);
bool getHeatingElement();

// Temperature control (on/off or PID)
void updateHeatingControl();

#endif // TEMPERATURE_H

