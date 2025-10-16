#ifndef DISPLAY_H
#define DISPLAY_H

/*
 * Display Module for ESP32-2432S028R (Cheap Yellow Display)
 * 
 * Hardware: 2.8" TFT LCD with Touch
 * Library: LVGL (LightWeight and Versatile Graphics Library)
 * 
 * Display Features:
 * - Power On/Off button
 * - Brew/Steam mode selector
 * - 4 Coffee size buttons (configured shot times)
 * - 2 Grinder size buttons (configured grind times)
 * 
 * Total: 8 touch buttons for simple operation
 * 
 * TODO: Implement when display hardware arrives
 * 1. Install LVGL library
 * 2. Configure TFT_eSPI for ESP32-2432S028R
 * 3. Design UI layout with 8 buttons
 * 4. Integrate with existing coffee station logic
 */

#include "config.h"

// Placeholder functions for future implementation

// Initialize display hardware and LVGL
void initDisplay();

// Update display with current system state
void updateDisplay();

// Handle touch input
void handleDisplayTouch();

// Display screens
void showMainScreen();
void showSettingsScreen();

// Future implementation notes:
// - Use LVGL widgets (lv_btn, lv_label)
// - Touch calibration on first boot
// - Show current temp, target temp, operation status
// - Visual feedback for button presses
// - Timeout to sleep/dim display

#endif // DISPLAY_H

