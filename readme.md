# ESP32 Simple OTA

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.x-blue.svg)](https://github.com/espressif/esp-idf)
[![Platform](https://img.shields.io/badge/platform-ESP32-lightgrey.svg)](https://www.espressif.com/en/products/socs/esp32)

A simple Over-The-Air (OTA) firmware update solution for ESP32 devices. This project provides a complete web-based interface for uploading firmware updates wirelessly via an ESP32 access point.

## Configuration

Configure the OTA service using `idf.py menuconfig` → **Simple OTA Configuration**:

| Setting | Default | Description |
|---------|---------|-------------|
| Access Point SSID | `Simple OTA` | WiFi network name |
| Access Point Password | `simpleota` | WiFi password (min 8 chars for WPA2) |
| mDNS Hostname | `simple-ota` | URL hostname (.local domain) |
| Auto-shutdown Timeout | `0` (disabled) | Minutes before auto-shutdown (0 = no timeout) |
| Auto-reboot | `Yes` | Reboot after successful update |
| Max File Size | `2 MB` | Maximum firmware file size |


## Core Functions

| Function | Description |
|----------|-------------|
| `simpleOTA_start()` | Start OTA with menuconfig settings |
| `simpleOTA_startWithConfig()` | Start OTA with runtime config |
| `simpleOTA_stop()` | Stop OTA service |
| `simpleOTA_validateOnBoot()` | Validate firmware on boot (call in app_main) |
| `simpleOTA_getStatus()` | Get current OTA status |
| `simpleOTA_setCallback()` | Set event callback for status updates |

See [`simpleOTA.h`](components/simpleOTA/include/simpleOTA.h) for complete API documentation.


## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.


