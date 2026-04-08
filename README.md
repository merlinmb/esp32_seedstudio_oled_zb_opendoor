# ESP32 Zigbee Door/Contact Sensor OLED Monitor

An ESP32 firmware that monitors Zigbee contact sensors (doors, windows, gates) via MQTT and displays their status on a 128×64 OLED screen. When everything is closed, animated "RoboEyes" are shown. When any sensor is open, the display switches to a scrolling list of open contacts.

## Features

- **MQTT subscription** to Zigbee2MQTT topics for any number of contact sensors
- **OLED display** (SH1106G 128×64, I2C) with two modes:
  - Animated RoboEyes when all contacts are closed
  - Scrolling list of open contacts with visual indicators
- **Touch sensor** (TTP223): single tap toggles display on/off, double-tap clears open-contact view, long press (5 s) cycles brightness
- **Web UI** served from the device for live configuration and OTA firmware updates
- **SPIFFS persistence** — all settings survive reboots via `/config.ini`
- **Non-blocking MQTT reconnect** with exponential back-off
- **NTP time sync**
- Supports multiple PlatformIO build environments (USB, OTA)

## Hardware

| Component | Details |
|-----------|---------|
| MCU | Seeed XIAO ESP32C3 (primary), WeMos D1 Mini32, or generic ESP32 |
| Display | SH1106G 128×64 OLED, I2C address `0x3C` |
| Touch | TTP223 capacitive sensor on `D2` / GPIO2 (optional, `#define ENABLE_TOUCH_SENSOR`) |

## Requirements

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- MQTT broker (e.g. Mosquitto) with Zigbee2MQTT publishing contact sensor payloads
- Arduino framework for ESP32 (`espressif32 @ 6.6.0`)

### Libraries (auto-installed by PlatformIO)

- `knolleary/PubSubClient`
- `PaulStoffregen/Time`
- `bblanchon/ArduinoJson`
- `taranais/NTPClient`
- `adafruit/Adafruit_SH110x` + `Adafruit-GFX-Library`
- `FluxGarage/RoboEyes`

## Setup

### 1. Configure credentials

Copy `include/connectionDetails.example.h` to `include/connectionDetails.h` and fill in your values:

```cpp
const char* SSID = "yourSSID";
const char* WIFIPASSWORD = "yourPassword";
const char* MQTT_SERVERADDRESS = "192.168.1.x";  // your broker IP
const char* MQTT_CLIENTNAME = "oledDoorMonitor";
const char* ARDUINO_OTA_UPDATE_PASSWORD = "yourOTApassword";
```

> `connectionDetails.h` is git-ignored — never commit real credentials.

### 2. Configure sensors

Edit `data/config.ini` before first flash (or update via the web UI afterwards):

```ini
flipscreen=0
brightness=100
open_scroll_interval_ms=1750
subscribetopics=zigbee2mqtt/yourbridge/0xAABBCCDDEEFF0011###Front door|zigbee2mqtt/yourbridge/0x112233445566778899###Garage
```

Each subscription entry is `topic###Friendly Name`, separated by `|`.

### 3. Build & flash

```bash
# USB flash to Seeed XIAO ESP32C3
pio run -e seeed_xiao_esp32c3 -t upload

# Upload filesystem (config.ini)
pio run -e seeed_xiao_esp32c3 -t uploadfs

# OTA update (set upload_port in platformio.ini first)
pio run -e esp32_ota -t upload
```

## Web UI

Once running, open `http://<device-ip>/` in a browser to:

- Add / remove MQTT subscriptions
- Adjust brightness and scroll speed
- Flip the display orientation
- Upload new firmware (password-protected)

## Touch Gestures

| Gesture | Action |
|---------|--------|
| Single tap | Toggle display on / off |
| Double tap | Return to RoboEyes view (clear open-contacts screen) |
| Long press (5 s) | Cycle through brightness levels |

## Configuration Reference (`config.ini`)

| Key | Default | Description |
|-----|---------|-------------|
| `flipscreen` | `0` | `1` to rotate display 180° |
| `brightness` | `100` | OLED contrast 0–255 |
| `open_scroll_interval_ms` | `1750` | ms between scroll steps when listing open contacts |
| `subscribetopics` | _(empty)_ | Pipe-delimited list of `topic###friendly` entries |

## Project Structure

```
├── src/
│   └── main.cpp              # Main firmware
├── include/
│   ├── connectionDetails.h   # WiFi / MQTT credentials (git-ignored)
│   ├── connectionDetails.example.h
│   ├── merlinNetwork.h       # WiFi helper
│   └── merlinUpdateWebServer.h # OTA web server
├── data/
│   └── config.ini            # SPIFFS config (uploaded separately)
└── platformio.ini
```

## License

MIT
