#ifndef _USER_SETUP_H_
#define _USER_SETUP_H_

// ============================================================================
// USER SETUP ID - Used to identify this configuration
// ============================================================================
#define USER_SETUP_ID 300

// ============================================================================
// DRIVER SELECTION - ILI9341 for ESP32-2432S028R
// ============================================================================
#define ILI9341_DRIVER
#define TFT_RGB_ORDER TFT_BGR
#define TFT_INVERSION_OFF

// ============================================================================
// ESP32-2432S028R PIN CONFIGURATION
// ============================================================================
// These pins are hardwired on the ESP32-2432S028R board
#define TFT_MISO 19  // Master In Slave Out (alternative pin)
#define TFT_MOSI 23  // Master Out Slave In (alternative pin)
#define TFT_SCLK 18  // Serial Clock (alternative pin)
#define TFT_CS   5   // Chip select control pin (alternative pin)
#define TFT_DC   2   // Data Command control pin
#define TFT_RST  4   // Reset pin
#define TFT_BL   21  // LED back-light (PWM controlled)

// ============================================================================
// DISPLAY SETTINGS
// ============================================================================
#define TFT_ROTATION 1  // Landscape mode like working code

// ============================================================================
// SPI SETTINGS
// ============================================================================
#define SPI_FREQUENCY  27000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000

// ============================================================================
// FONT SETTINGS
// ============================================================================
#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH
#define LOAD_FONT6  // Font 6. Large 48 pixel high font, needs ~2666 bytes in FLASH
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel high font, needs ~2438 bytes in FLASH
#define LOAD_FONT8  // Font 8. Large 75 pixel high font needs ~3256 bytes in FLASH
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

// ============================================================================
// SMOOTH FONT SETTINGS
// ============================================================================
#define SMOOTH_FONT

#endif // _USER_SETUP_H_