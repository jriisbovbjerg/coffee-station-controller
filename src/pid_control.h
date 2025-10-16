#ifndef PID_CONTROL_H
#define PID_CONTROL_H

#include <Arduino.h>
#include "config.h"

// External dependencies
extern CoffeeConfig coffeeConfig;
extern SystemState systemState;

// Forward declaration of heating control
void setHeatingElement(bool state);

// Initialize PID controller
void initPID();

// Update PID tuning parameters
void updatePIDTunings(float kp, float ki, float kd);

// PID control update (called from temperature module)
void updatePIDControl(float currentTemp, float targetTemp);

// AutoTune functions
void startAutotune();
void stopAutotune(bool saveResults);
void updateAutotune();
bool isAutotuning();

#endif // PID_CONTROL_H

