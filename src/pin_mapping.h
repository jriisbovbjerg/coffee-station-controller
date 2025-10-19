#ifndef PIN_MAPPING_H
#define PIN_MAPPING_H

// ESP32-2432S028R Pin Mapping
// This file defines GPIO assignments compatible with the built-in display

// ============================================================================
// TEMPERATURE SENSOR (MAX31855) - Remapped to avoid display conflicts
// ============================================================================
#define MAX31855_CS_PIN   16  // Chip Select pin (was GPIO 5, now GPIO 16)
#define MAX31855_CLK_PIN  17  // Clock pin (was GPIO 18, now GPIO 17)  
#define MAX31855_DO_PIN   27  // Data Out pin (was GPIO 19, now GPIO 27)

// ============================================================================
// HEATING ELEMENT CONTROL - Remapped to avoid touch/SD conflicts
// ============================================================================
#define SSR_HEATING_PIN  26  // SSR control (was GPIO 2)

// ============================================================================
// DISPLAY PINS (Internal to ESP32-2432S028R - DO NOT CHANGE)
// ============================================================================
// TFT Display uses internal SPI bus:
// - GPIO 5, 18, 19, 23 (SPI)
// - GPIO 21, 22 (DC, RST)
// - GPIO 15 (Backlight)

// Touch Controller:
// - GPIO 2, 4 (Touch I2C)

// SD Card:
// - GPIO 15, 2 (SD SPI)

// ============================================================================
// AVAILABLE GPIOs FOR FUTURE EXPANSION
// ============================================================================
// GPIO 0, 4, 16, 17, 21, 22, 23, 25, 26, 27, 32, 33
// Note: Some may be used by display internally

// ============================================================================
// PIN VALIDATION
// ============================================================================
// Verify these pins don't conflict with display:
// - MAX31855: 16, 17, 27 ✓ (safe)
// - SSR: 26 ✓ (safe)
// - All pins are 3.3V compatible ✓

#endif // PIN_MAPPING_H
