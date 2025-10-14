# Coffee Station Controller

ESP32-based controller for monitoring and controlling coffee brewing equipment with temperature sensing and InfluxDB logging.

## Features

- **K-Type Temperature Monitoring**: High-precision temperature sensing using MAX31855 thermocouple amplifier
- **WiFi Connectivity**: Connects to local network for remote monitoring and OTA updates
- **InfluxDB Integration**: Real-time temperature data logging for Grafana dashboard visualization
- **Over-The-Air Updates**: Remote firmware updates via Arduino OTA
- **mDNS Support**: Easy network discovery as `coffee.local`
- **Serial Monitoring**: Real-time temperature readings via serial console

## Hardware Requirements

### Main Components
- ESP32 development board (e.g., UPESY WROOM)
- Adafruit MAX31855 K-Type Thermocouple Amplifier breakout board
- K-Type thermocouple sensor
- Jumper wires
- Breadboard or PCB for connections

### Pin Connections

| MAX31855 Breakout | ESP32 Pin | Description |
|-------------------|-----------|-------------|
| VCC               | 3.3V      | Power supply |
| GND               | GND       | Ground |
| CS                | GPIO 5    | Chip Select |
| CLK               | GPIO 18   | Clock |
| DO                | GPIO 19   | Data Out |

## Software Requirements

- [PlatformIO](https://platformio.org/) IDE or extension
- ESP32 board package
- Required libraries (automatically installed via platformio.ini):
  - Adafruit MAX31855 library
  - PubSubClient (for future MQTT support)

## Installation & Setup

1. **Clone the repository**:
   ```bash
   git clone <repository-url>
   cd coffee_station
   ```

2. **Update WiFi credentials**:
   Edit `src/main.cpp` and update:
   ```cpp
   const char* ssid = "your-wifi-network";
   const char* password = "your-wifi-password";
   ```

3. **Configure InfluxDB settings**:
   Update the InfluxDB host IP in `src/main.cpp`:
   ```cpp
   byte udp_host[] = {192, 168, 10, 7}; // Your InfluxDB server IP
   ```

4. **Build and upload**:
   ```bash
   platformio run --target upload
   ```

## Configuration

### Temperature Sensor
- **Resolution**: 0.25°C (hardware limitation of MAX31855)
- **Range**: -200°C to +1350°C (K-type thermocouple dependent)
- **Update Rate**: 1 second (configurable via `interval` variable)

### InfluxDB Data Format
Temperature data is sent to InfluxDB in the following format:
```
temp,host=<device-hostname>,location=coffee-brew-01 value=<temperature>
```

Example:
```
temp,host=coffee,location=coffee-brew-01 value=23.25
```

### Network Services
- **mDNS hostname**: `coffee.local`
- **OTA updates**: Available on the same hostname
- **InfluxDB UDP**: Port 8089

## Usage

1. **Power up the device** - it will automatically connect to WiFi
2. **Monitor via Serial**: Connect at 115200 baud to see live temperature readings
3. **Network discovery**: The device is accessible at `coffee.local`
4. **Grafana monitoring**: Temperature data flows to InfluxDB for dashboard visualization
5. **OTA updates**: Use PlatformIO to upload new firmware over WiFi

### Serial Output Example
```
WiFi connected successfully with IP-address:192.168.10.155
mDNS responder started: coffee.local
OTA ready. Flash with hostname: coffee.local
InfluxDB will use hostname: coffee
Initializing MAX31855 K-type thermocouple sensor...
Initial temperature reading: 23.25°C
Coffee Temperature: 23.50°C
Coffee Temperature: 23.25°C
```

## Error Handling

The system includes comprehensive error detection for the thermocouple:
- **Open circuit**: No thermocouple connected
- **Short to ground**: Thermocouple wire shorted to ground
- **Short to VCC**: Thermocouple wire shorted to power
- **Invalid readings**: NaN values are filtered out

## Future Enhancements

- PID temperature control for heating elements
- Grinder control integration  
- Web interface for configuration
- MQTT support for home automation
- Multiple temperature sensor support
- Data logging to SD card

## Development

### Project Structure
```
coffee_station/
├── src/
│   └── main.cpp          # Main application code
├── include/              # Header files (currently empty)
├── lib/                  # Local libraries
├── test/                 # Unit tests
├── platformio.ini        # PlatformIO configuration
└── README.md            # This file
```

### Building
```bash
# Build only
platformio run

# Build and upload
platformio run --target upload

# Serial monitor
platformio device monitor
```

## License

This project is open source. Feel free to modify and distribute as needed.

## Contributing

Pull requests and issues are welcome! Please ensure any contributions maintain the existing code style and include appropriate documentation.