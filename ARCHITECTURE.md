# Coffee Station Controller - Architecture Documentation

## Overview
ESP32-based coffee station controller with PID temperature control, web interface, and future local display support.

## Hardware
- **MCU:** ESP32 (uPesy Wroom DevKit)
- **Temperature Sensor:** MAX31855 K-Type Thermocouple
- **Heating Control:** SSR-DA Relay (GPIO 2)
- **Display (Future):** ESP32-2432S028R 2.8" TFT LCD with Touch

## Modular Architecture

```
src/
├── config.h              - Configuration structures and state
├── main.cpp              - Setup, loop, and coordination
├── temperature.h/.cpp    - Temperature sensor and heating control
├── pid_control.h/.cpp    - PID controller and autotune
├── storage.h/.cpp        - Configuration persistence (NVS)
├── web_server.h/.cpp     - REST API endpoints
├── web_pages.h           - HTML/CSS/JavaScript interface
└── display.h             - [Future] LVGL display interface
```

## Module Responsibilities

### 1. `config.h`
**Purpose:** Centralized configuration and state structures

**Contents:**
- `CoffeeConfig` - All user-configurable parameters
- `SystemState` - Runtime system state

**Key Settings:**
- Temperature setpoints (brew: 93°C, steam: 150°C)
- Shot sizes (4 configurable times)
- Grind times (2 configurable times)
- PID parameters (Kp, Ki, Kd)
- System preferences (InfluxDB, update intervals)

### 2. `temperature.h/.cpp`
**Purpose:** Hardware abstraction for temperature sensing and heating

**Functions:**
- `initTemperatureSensor()` - Initialize MAX31855 and heating pin
- `readTemperature()` - Read current temperature with error handling
- `setHeatingElement(bool)` - Control SSR relay
- `updateHeatingControl()` - Delegate to on/off or PID control

**Hardware Pins:**
- MAX31855 CS: GPIO 5
- MAX31855 CLK: GPIO 18
- MAX31855 DO: GPIO 19
- Heating SSR: GPIO 2

### 3. `pid_control.h/.cpp`
**Purpose:** Advanced temperature control algorithms

**Functions:**
- `initPID()` - Initialize PID controller
- `updatePIDTunings(kp, ki, kd)` - Update PID parameters
- `updatePIDControl(current, target)` - Execute PID algorithm
- `startAutotune()` - Begin Ziegler-Nichols autotuning
- `stopAutotune(saveResults)` - End autotuning
- `updateAutotune()` - Run autotune step
- `isAutotuning()` - Check autotune status

**Libraries:**
- `PID_v1` by Brett Beauregard
- `sTune` for autotune functionality

**Control Modes:**
- **On/Off:** Simple hysteresis (±1°C)
- **PID:** Smooth control with configurable parameters

### 4. `storage.h/.cpp`
**Purpose:** Configuration persistence using ESP32 NVS (Preferences)

**Functions:**
- `initStorage()` - Initialize storage system
- `saveConfiguration()` - Write config to flash
- `loadConfiguration()` - Read config from flash

**Stored Parameters:**
- All temperature setpoints
- Shot and grind times
- PID parameters
- System preferences

**Namespace:** `"coffee-config"`

### 5. `web_server.h/.cpp`
**Purpose:** REST API and web interface

**Endpoints:**
| Method | Path | Description |
|--------|------|-------------|
| GET | `/` | Serve HTML interface |
| GET | `/api/status` | Current system state |
| GET | `/api/config` | Get configuration |
| POST | `/api/config` | Update configuration |
| POST | `/api/heating/toggle` | Toggle heating element |
| POST | `/api/mode/brew` | Set brew mode |
| POST | `/api/mode/steam` | Set steam mode |
| POST | `/api/autotune/start` | Start PID autotune |
| POST | `/api/autotune/stop` | Stop PID autotune |
| GET | `/api/autotune/status` | Autotune status |

**Libraries:**
- `ESPAsyncWebServer`
- `AsyncTCP`
- `ArduinoJson` v7

### 6. `web_pages.h`
**Purpose:** HTML/CSS/JavaScript user interface

**Features:**
- Real-time status display (2-second updates)
- Temperature configuration
- Shot and grind time settings
- PID parameter tuning
- AutoTune controls with progress display
- System settings (InfluxDB, update interval)

### 7. `main.cpp`
**Purpose:** System initialization and coordination

**Responsibilities:**
- WiFi connection
- mDNS setup (coffee.local)
- OTA updates with priority handling
- InfluxDB data transmission
- Main control loop coordination

**Control Flow:**
1. OTA handling (highest priority)
2. Temperature reading (every 2 seconds)
3. InfluxDB logging
4. Heating control (autotune or normal)

## Data Flow

```
┌─────────────┐
│ Temperature │
│   Sensor    │
└──────┬──────┘
       │
       ▼
┌─────────────┐     ┌──────────┐
│ Temperature │────▶│   PID    │
│   Module    │     │ Control  │
└──────┬──────┘     └────┬─────┘
       │                 │
       │    ┌────────────┘
       │    │
       ▼    ▼
┌─────────────┐
│   Heating   │
│   Element   │
└─────────────┘

       │
       ▼
┌─────────────┐     ┌──────────┐
│  InfluxDB   │     │   Web    │
│  (Logging)  │     │   API    │
└─────────────┘     └──────────┘
```

## Future: Display Integration

### Hardware: ESP32-2432S028R ("Cheap Yellow Display")
- 2.8" ILI9341 TFT LCD (320x240)
- XPT2046 Touch Controller
- Built-in SD card slot
- USB-C connector

### Library: LVGL (LightWeight and Versatile Graphics Library)
- Version: 8.x or 9.x
- Companion: TFT_eSPI for hardware driver

### Display UI Design

**Main Screen Layout:**
```
┌─────────────────────────────────────┐
│  ☕ Coffee Station     23.5°C/93°C  │
├─────────────────────────────────────┤
│                                     │
│  ┌──────────┐     ┌──────────┐     │
│  │  POWER   │     │  BREW /  │     │
│  │  ON/OFF  │     │  STEAM   │     │
│  └──────────┘     └──────────┘     │
│                                     │
│  Coffee Size:                       │
│  ┌────┐ ┌────┐ ┌────┐ ┌────┐       │
│  │ S  │ │ M  │ │ L  │ │ XL │       │
│  └────┘ └────┘ └────┘ └────┘       │
│                                     │
│  Grind:                             │
│  ┌──────────┐  ┌──────────┐        │
│  │  Single  │  │  Double  │        │
│  └──────────┘  └──────────┘        │
│                                     │
└─────────────────────────────────────┘
```

### Integration Steps (when hardware arrives)

1. **Add Libraries to `platformio.ini`:**
```ini
lib_deps =
    ...
    lvgl/lvgl@^8.3.0
    bodmer/TFT_eSPI@^2.5.0
```

2. **Configure TFT_eSPI:**
- Create `User_Setup.h` for ESP32-2432S028R
- Set pins and display type

3. **Implement `display.cpp`:**
- Initialize LVGL and TFT_eSPI
- Create button widgets
- Link buttons to existing functions:
  - Power → `setHeatingElement()`
  - Mode → `systemState.steamMode`
  - Sizes → Shot/grind times from `coffeeConfig`

4. **Update `main.cpp` loop:**
```cpp
void loop() {
    ArduinoOTA.handle();
    if (otaInProgress) return;
    
    // Add display handling
    lv_timer_handler();  // LVGL task handler
    handleDisplayTouch(); // Process touch input
    updateDisplay();     // Refresh display
    
    // Existing temperature control...
}
```

5. **No Changes Needed:**
- Temperature module ✓
- PID control ✓
- Storage ✓
- Web server ✓

The modular architecture allows display integration without modifying existing modules!

## Benefits of Current Architecture

✅ **Separation of Concerns** - Each module has single responsibility  
✅ **Easy Testing** - Modules can be tested independently  
✅ **Maintainability** - Changes isolated to specific modules  
✅ **Extensibility** - Add display without touching existing code  
✅ **Reusability** - Modules can be used in other projects  
✅ **Clean Dependencies** - Clear module interactions  

## Configuration Files

### `platformio.ini`
```ini
[env:upesy_wroom]
platform = espressif32
board = upesy_wroom
framework = arduino
upload_protocol = espota
upload_port = 192.168.10.155  # or coffee.local

lib_deps =
    PubSubClient
    Adafruit MAX31855 library
    ESPAsyncWebServer
    AsyncTCP
    ArduinoJson@^7.0.4
    PID@^1.2.1
    sTune
```

## Memory Usage

**Current (without display):**
- Flash: 68.1% (893,221 bytes)
- RAM: 15.3% (50,032 bytes)

**Estimated with LVGL:**
- Flash: ~75-80% (+100-150KB for LVGL)
- RAM: ~25-30% (+40-50KB for frame buffers)

Plenty of room for display integration! ✓

## Development Workflow

1. **Local Development:**
   - Edit code in VSCode/Cursor
   - Build with PlatformIO
   - Upload via OTA (coffee.local)

2. **Adding Features:**
   - Identify appropriate module
   - Add function to module
   - Update `main.cpp` if needed
   - Update web interface if needed
   - Compile and test

3. **Display Development (Future):**
   - Design UI in LVGL simulator first
   - Implement in `display.cpp`
   - Integrate with `main.cpp` loop
   - Test on hardware

## Network Services

- **mDNS:** `coffee.local`
- **Web Server:** Port 80
- **OTA Updates:** Port 3232
- **InfluxDB:** UDP to 192.168.10.7:8089

## Logging

- **Serial:** 115200 baud
- **InfluxDB:** Every 2 seconds
  - `coffee-brew-01`: Current temperature
  - `coffe_target-01`: Target temperature

## Safety Features

- Sensor fault detection (MAX31855)
- Emergency stop on sensor failure
- AutoTune timeout (10 minutes)
- AutoTune emergency stop (target + 10°C)
- OTA priority mode (suspends heating during updates)

---

**Ready for Display Integration!** 🎉

When the ESP32-2432S028R arrives, we can add the display module without refactoring the existing clean architecture.

