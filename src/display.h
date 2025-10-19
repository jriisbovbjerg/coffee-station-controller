#ifndef DISPLAY_H
#define DISPLAY_H

#include <lvgl.h>
#include <TFT_eSPI.h>
#include "config.h"

// ============================================================================
// DISPLAY CONFIGURATION
// ============================================================================
#define DISPLAY_WIDTH  240
#define DISPLAY_HEIGHT 320
#define LVGL_TICK_PERIOD_MS 5

// ============================================================================
// EXTERNAL DEPENDENCIES
// ============================================================================
extern CoffeeConfig coffeeConfig;
extern SystemState systemState;
extern void setHeatingElement(bool state);
extern void saveConfiguration();

// ============================================================================
// DISPLAY FUNCTIONS
// ============================================================================
void initDisplay();
void updateDisplay();
void handleDisplayTouch();
void lvglTick();

// ============================================================================
// UI ELEMENT FUNCTIONS
// ============================================================================
void createMainUI();
void updateTemperatureDisplay();
void updateModeDisplay();
void updateShotSizeDisplay();
void updateGrindTimeDisplay();
void updatePowerButton();

// ============================================================================
// TOUCH HANDLERS
// ============================================================================
void onPowerButtonPressed(lv_event_t * e);
void onModeButtonPressed(lv_event_t * e);
void onShotSizePressed(lv_event_t * e);
void onGrindTimePressed(lv_event_t * e);

#endif // DISPLAY_H

