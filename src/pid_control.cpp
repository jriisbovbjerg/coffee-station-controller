#include "pid_control.h"
#include <PID_v1.h>
#include <sTune.h>

// Forward declaration for saving configuration
void saveConfiguration();

// ======= PID Control Variables =======
static double pidInput = 0.0;      // Current temperature (input to PID)
static double pidOutput = 0.0;     // PID output (0-255)
static double pidSetpoint = 0.0;   // Target temperature (setpoint for PID)
static PID heatingPID(&pidInput, &pidOutput, &pidSetpoint, 
                      2.0, 5.0, 1.0, DIRECT);  // Initial values, will be updated

// ======= PID AutoTune Variables =======
static float tuneInput = 0.0;
static float tuneOutput = 0.0;
static sTune tuner = sTune(&tuneInput, &tuneOutput, sTune::ZN_PID, sTune::directIP, sTune::printOFF);
static bool autotuning = false;
static unsigned long autotuneStartTime = 0;
static const unsigned long AUTOTUNE_TIMEOUT = 600000; // 10 minutes timeout

// ======= PID Initialization =======
void initPID() {
  heatingPID.SetMode(AUTOMATIC);
  heatingPID.SetOutputLimits(0, 255);
  heatingPID.SetSampleTime(1000); // 1 second sample time
  heatingPID.SetTunings(coffeeConfig.pidKp, coffeeConfig.pidKi, coffeeConfig.pidKd);
  
  Serial.println("PID controller initialized");
  Serial.printf("PID Parameters: Kp=%.3f, Ki=%.3f, Kd=%.3f, Mode=%s\n", 
                coffeeConfig.pidKp, coffeeConfig.pidKi, coffeeConfig.pidKd,
                coffeeConfig.usePID ? "PID" : "On/Off");
}

// ======= Update PID Tunings =======
void updatePIDTunings(float kp, float ki, float kd) {
  coffeeConfig.pidKp = kp;
  coffeeConfig.pidKi = ki;
  coffeeConfig.pidKd = kd;
  heatingPID.SetTunings(kp, ki, kd);
  Serial.printf("PID tunings updated: Kp=%.3f, Ki=%.3f, Kd=%.3f\n", kp, ki, kd);
}

// ======= PID Control Update =======
void updatePIDControl(float currentTemp, float targetTemp) {
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
}

// ======= AutoTune Functions =======
void startAutotune() {
  if (autotuning) {
    Serial.println("Autotune already running!");
    return;
  }
  
  // Configure autotune for espresso machine
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

bool isAutotuning() {
  return autotuning;
}

