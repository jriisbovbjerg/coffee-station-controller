#include "display.h"
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// Touch calibration mode
static bool calibrationMode = true;
static int calibrationStep = 0;
static unsigned long lastTouchDebug = 0;

// ============================================================================
// DISPLAY HARDWARE
// ============================================================================
static TFT_eSPI tft = TFT_eSPI();

// Touch screen (XPT2046) - separate SPI bus
#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK 25

static XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[DISPLAY_WIDTH * 10];
static lv_color_t buf2[DISPLAY_WIDTH * 10];

// ============================================================================
// UI ELEMENTS
// ============================================================================
static lv_obj_t *main_screen;
static lv_obj_t *temp_label;
static lv_obj_t *target_label;
static lv_obj_t *power_btn;
static lv_obj_t *mode_btn;
static lv_obj_t *shot_btns[4];
static lv_obj_t *grind_btns[2];
static lv_obj_t *status_label;

// ============================================================================
// LVGL TOUCH INPUT CALLBACK
// ============================================================================
void lvgl_touch_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    if (touch.touched()) {
        TS_Point p = touch.getPoint();
        
        // Only process if pressure is reasonable
        if (p.z > 200 && p.z < 4000) {
            // Always show raw touch coordinates for calibration
            if (millis() - lastTouchDebug > 200) {  // Throttle output
                lastTouchDebug = millis();
                Serial.printf("\n=== TOUCH DEBUG ===");
                Serial.printf("\nRaw touch: X=%d, Y=%d, Pressure=%d", p.x, p.y, p.z);
                
                if (calibrationMode) {
                    Serial.printf("\n[CALIBRATION STEP %d]", calibrationStep);
                    switch(calibrationStep) {
                        case 0: Serial.print(" -> Please touch TOP-LEFT corner"); break;
                        case 1: Serial.print(" -> Please touch TOP-RIGHT corner"); break;
                        case 2: Serial.print(" -> Please touch BOTTOM-RIGHT corner"); break;
                        case 3: Serial.print(" -> Please touch BOTTOM-LEFT corner"); break;
                        case 4: Serial.print(" -> Calibration complete! Reboot to use."); break;
                    }
                    if (calibrationStep < 4) {
                        calibrationStep++;
                    }
                }
                Serial.println();
            }
            
            // Calibrated mapping based on actual touch data
            // X: right(~300) to left(~3800) -> display 240 to 0 (inverted)
            // Y: bottom(~300) to top(~3800) -> display 320 to 0 (inverted)
            data->point.x = map(p.x, 300, 3800, 240, 0);
            data->point.y = map(p.y, 300, 3800, 320, 0);
            data->state = LV_INDEV_STATE_PRESSED;
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ============================================================================
// LVGL DISPLAY FLUSH CALLBACK
// ============================================================================
void lvgl_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

// ============================================================================
// LVGL TICK INCREMENT (called from Arduino loop)
// ============================================================================
void lvglTick() {
    // Not needed - LV_TICK_CUSTOM is enabled in lv_conf.h
    // LVGL automatically uses millis() for tick timing
}

// ============================================================================
// BUTTON EVENT HANDLERS
// ============================================================================
void onPowerButtonPressed(lv_event_t * e) {
    bool newState = !systemState.heatingElement;
    setHeatingElement(newState);
    updatePowerButton();
    
    Serial.print("Power button pressed - Heating: ");
    Serial.println(newState ? "ON" : "OFF");
}

void onModeButtonPressed(lv_event_t * e) {
    systemState.steamMode = !systemState.steamMode;
    systemState.targetTemp = systemState.steamMode ? coffeeConfig.steamTemp : coffeeConfig.brewTemp;
    updateModeDisplay();
    saveConfiguration();
    
    Serial.print("Mode changed to: ");
    Serial.println(systemState.steamMode ? "STEAM" : "BREW");
}

void onShotSizePressed(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    
    // Find which button was pressed
    for (int i = 0; i < 4; i++) {
        if (btn == shot_btns[i]) {
            systemState.selectedShotSize = i;
            updateShotSizeDisplay();
            Serial.printf("Shot size selected: %s (%.1fs)\n", 
                         coffeeConfig.shotNames[i], coffeeConfig.shotSizes[i]);
            break;
        }
    }
}

void onGrindTimePressed(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    
    // Find which button was pressed
    for (int i = 0; i < 2; i++) {
        if (btn == grind_btns[i]) {
            systemState.selectedGrindTime = i;
            updateGrindTimeDisplay();
            Serial.printf("Grind time selected: %s (%.1fs)\n", 
                         coffeeConfig.grindNames[i], coffeeConfig.grindTimes[i]);
            break;
        }
    }
}

// ============================================================================
// UI UPDATE FUNCTIONS
// ============================================================================
void updateTemperatureDisplay() {
    if (!temp_label || !target_label) return;
    
    char temp_str[32];
    snprintf(temp_str, sizeof(temp_str), "%.1f°C", systemState.currentTemp);
    lv_label_set_text(temp_label, temp_str);
    
    char target_str[32];
    snprintf(target_str, sizeof(target_str), "Target:%.0f°C", systemState.targetTemp);
    lv_label_set_text(target_label, target_str);
}

void updateModeDisplay() {
    if (!mode_btn) return;
    
    const char* mode_text = systemState.steamMode ? "STEAM" : "BREW";
    lv_label_set_text(lv_obj_get_child(mode_btn, 0), mode_text);
    
    // Change color based on mode
    lv_obj_t *label = lv_obj_get_child(mode_btn, 0);
    if (systemState.steamMode) {
        lv_obj_set_style_bg_color(mode_btn, lv_color_hex(0xFF0000), 0); // Pure red for steam
    } else {
        lv_obj_set_style_bg_color(mode_btn, lv_color_hex(0x0099FF), 0); // Pure blue for brew
    }
}

void updateShotSizeDisplay() {
    for (int i = 0; i < 4; i++) {
        if (!shot_btns[i]) continue;
        
        if (i == systemState.selectedShotSize) {
            lv_obj_set_style_bg_color(shot_btns[i], lv_color_hex(0x00FF00), 0); // Pure green when selected
        } else {
            lv_obj_set_style_bg_color(shot_btns[i], lv_color_hex(0x808080), 0); // Medium gray when not selected
        }
    }
}

void updateGrindTimeDisplay() {
    for (int i = 0; i < 2; i++) {
        if (!grind_btns[i]) continue;
        
        if (i == systemState.selectedGrindTime) {
            lv_obj_set_style_bg_color(grind_btns[i], lv_color_hex(0x00FF00), 0); // Pure green when selected
        } else {
            lv_obj_set_style_bg_color(grind_btns[i], lv_color_hex(0x808080), 0); // Medium gray when not selected
        }
    }
}

void updatePowerButton() {
    if (!power_btn) return;
    
    const char* power_text = systemState.heatingElement ? "POWER\nON" : "POWER\nOFF";
    lv_label_set_text(lv_obj_get_child(power_btn, 0), power_text);
    
    // Change color based on state
    if (systemState.heatingElement) {
        lv_obj_set_style_bg_color(power_btn, lv_color_hex(0x00FF00), 0); // Pure green when on
    } else {
        lv_obj_set_style_bg_color(power_btn, lv_color_hex(0x808080), 0); // Medium gray when off
    }
}

// ============================================================================
// MAIN UI CREATION
// ============================================================================
void createMainUI() {
    main_screen = lv_obj_create(NULL);
    lv_scr_load(main_screen);
    lv_obj_set_style_bg_color(main_screen, lv_color_hex(0x2C3E50), 0);
    
    // ========== HEADER: Title and Temperature ==========
    lv_obj_t *header = lv_obj_create(main_screen);
    lv_obj_set_size(header, 230, 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x1A252F), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 5, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);  // Disable scrolling
    
    // Title (left side)
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "Coffee Station");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0); // White text
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 2, 0);
    
    // Current temperature (right side, larger)
    temp_label = lv_label_create(header);
    lv_label_set_text(temp_label, "--°C");
    lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(temp_label, lv_color_hex(0xE67E22), 0);
    lv_obj_align(temp_label, LV_ALIGN_RIGHT_MID, -2, -6);
    
    // Target temperature (smaller, below current temp)
    target_label = lv_label_create(header);
    lv_label_set_text(target_label, "Target:--°C");
    lv_obj_set_style_text_font(target_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(target_label, lv_color_hex(0xBDC3C7), 0);
    lv_obj_align(target_label, LV_ALIGN_RIGHT_MID, -2, 9);
    
    // ========== CONTROL BUTTONS ROW ==========
    // Power button
    power_btn = lv_btn_create(main_screen);
    lv_obj_set_size(power_btn, 110, 60);
    lv_obj_align(power_btn, LV_ALIGN_TOP_LEFT, 5, 60);
    lv_obj_add_event_cb(power_btn, onPowerButtonPressed, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(power_btn, lv_color_hex(0x808080), 0);
    
    lv_obj_t *power_label = lv_label_create(power_btn);
    lv_label_set_text(power_label, "POWER\nOFF");
    lv_obj_center(power_label);
    
    // Mode button (Brew/Steam)
    mode_btn = lv_btn_create(main_screen);
    lv_obj_set_size(mode_btn, 110, 60);
    lv_obj_align(mode_btn, LV_ALIGN_TOP_RIGHT, -5, 60);
    lv_obj_add_event_cb(mode_btn, onModeButtonPressed, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(mode_btn, lv_color_hex(0x0099FF), 0);
    
    lv_obj_t *mode_label = lv_label_create(mode_btn);
    lv_label_set_text(mode_label, "BREW");
    lv_obj_center(mode_label);
    
    // ========== SHOT SIZE SECTION ==========
    lv_obj_t *shot_title = lv_label_create(main_screen);
    lv_label_set_text(shot_title, "Shot Size:");
    lv_obj_set_style_text_font(shot_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(shot_title, lv_color_hex(0xFFFFFF), 0); // White text
    lv_obj_align(shot_title, LV_ALIGN_TOP_LEFT, 10, 130);
    
    const char* shot_labels[] = {"S", "M", "L", "XL"};
    for (int i = 0; i < 4; i++) {
        shot_btns[i] = lv_btn_create(main_screen);
        lv_obj_set_size(shot_btns[i], 50, 50);
        lv_obj_align(shot_btns[i], LV_ALIGN_TOP_LEFT, 10 + (i * 55), 155);
        lv_obj_add_event_cb(shot_btns[i], onShotSizePressed, LV_EVENT_CLICKED, NULL);
        lv_obj_set_style_bg_color(shot_btns[i], lv_color_hex(0x808080), 0);
        
        lv_obj_t *label = lv_label_create(shot_btns[i]);
        lv_label_set_text(label, shot_labels[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
        lv_obj_center(label);
    }
    
    // ========== GRIND TIME SECTION ==========
    lv_obj_t *grind_title = lv_label_create(main_screen);
    lv_label_set_text(grind_title, "Grind:");
    lv_obj_set_style_text_font(grind_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(grind_title, lv_color_hex(0xFFFFFF), 0); // White text
    lv_obj_align(grind_title, LV_ALIGN_TOP_LEFT, 10, 215);
    
    const char* grind_labels[] = {"Single", "Double"};
    for (int i = 0; i < 2; i++) {
        grind_btns[i] = lv_btn_create(main_screen);
        lv_obj_set_size(grind_btns[i], 105, 50);
        lv_obj_align(grind_btns[i], LV_ALIGN_TOP_LEFT, 10 + (i * 110), 240);
        lv_obj_add_event_cb(grind_btns[i], onGrindTimePressed, LV_EVENT_CLICKED, NULL);
        lv_obj_set_style_bg_color(grind_btns[i], lv_color_hex(0x808080), 0);
        
        lv_obj_t *label = lv_label_create(grind_btns[i]);
        lv_label_set_text(label, grind_labels[i]);
        lv_obj_center(label);
    }
    
    // ========== STATUS BAR ==========
    status_label = lv_label_create(main_screen);
    lv_label_set_text(status_label, "Ready");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x95A5A6), 0);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    // Initialize UI state
    updatePowerButton();
    updateModeDisplay();
    updateShotSizeDisplay();
    updateGrindTimeDisplay();
    updateTemperatureDisplay();
    
    Serial.println("UI created successfully");
}

// ============================================================================
// DISPLAY INITIALIZATION
// ============================================================================
void initDisplay() {
    Serial.println("Initializing TFT display...");
    
    // Initialize TFT
    tft.begin();
    tft.setRotation(1); // Landscape mode
    tft.fillScreen(TFT_BLACK);
    
    // Test backlight (GPIO 21 on ESP32-2432S028R)
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);
    
    Serial.println("Initializing LVGL...");
    
    // Initialize LVGL
    lv_init();
    
    // Setup display buffer
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISPLAY_WIDTH * 10);
    
    // Initialize display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = DISPLAY_WIDTH;
    disp_drv.ver_res = DISPLAY_HEIGHT;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    
    // Initialize touch controller (separate SPI bus)
    Serial.println("Initializing touch controller...");
    SPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    touch.begin();
    touch.setRotation(0);  // Use raw orientation; we handle mapping/rotation in code
    
    // Register touch input device with LVGL
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_read;
    lv_indev_drv_register(&indev_drv);
    
    Serial.println("Display and touch initialized successfully");
    
    Serial.println("\n\n===========================================");
    Serial.println("   TOUCH CALIBRATION MODE");
    Serial.println("===========================================");
    Serial.println("Please touch the screen at each corner when prompted.");
    Serial.println("This will help identify correct touch mapping.\n");
    
    // Create the UI
    createMainUI();
}

// ============================================================================
// DISPLAY UPDATE (call regularly from main loop)
// ============================================================================
void updateDisplay() {
    lv_timer_handler();
    updateTemperatureDisplay();
    
    // Update status label with current operation
    if (status_label) {
        lv_label_set_text(status_label, systemState.currentOperation.c_str());
    }
}

// ============================================================================
// TOUCH HANDLING (placeholder for now)
// ============================================================================
void handleDisplayTouch() {
    // Touch is handled automatically by LVGL event system
    // This function is here for future touch-specific processing if needed
}
