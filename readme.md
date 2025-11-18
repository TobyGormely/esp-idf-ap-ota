# ESP32 Simple OTA

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.x-blue.svg)](https://github.com/espressif/esp-idf)
[![Platform](https://img.shields.io/badge/platform-ESP32-lightgrey.svg)](https://www.espressif.com/en/products/socs/esp32)

A simple Over-The-Air (OTA) firmware update solution for ESP32 devices. This project provides a complete web-based interface for uploading firmware updates wirelessly via an ESP32 access point.

## Usage

1. **Flash and run** the firmware on your ESP32 device
2. **Connect** to the WiFi access point created by the device (default: `Simple OTA`)
3. **Access the web interface** via browser:
   - `http://simple-ota.local` (mDNS hostname)
   - `http://10.0.0.1` (direct IP address)
4. **Upload firmware** by dragging a `.bin` file to the interface or clicking to browse
5. **Wait for completion** - the device will automatically validate and reboot

The web interface provides drag-and-drop file upload, real-time progress tracking, and automatic firmware validation with rollback protection.

![ESP32 Simple OTA](pageScreenshot.png)
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

### Web Interface Customisation

Personalise the OTA web interface through **Simple OTA Configuration** → **Web Page Customisation**:

| Setting | Default | Description |
|---------|---------|-------------|
| Web Page Title | `Simple OTA` | Browser title and main heading |
| Web Page Footer | `Created By Toby Gormely` | Footer text displayed in interface |
| Primary Colour | `0x007bff` | Main Colour for buttons and progress bars |
| Secondary Colour | `0x6c757d` | Border and text colors |
| Background Colour | `0xf8f9fa` | Page and component backgrounds |

**Colour Format**: Use hexadecimal values (e.g., `0xff5722` for orange, `0x4caf50` for green). Colors automatically adapt across the interface while maintaining accessibility and visual consistency.

**Logo Customisation**: Replace the default logo by updating `data/logo.png` with your own image. 

**Partition Requirements**: OTA functionality requires dual app partitions. Ensure your partition table includes `ota_0` and `ota_1` partitions. Use `idf.py menuconfig` → **Partition Table** → **Default 2MB two OTA** or configure a custom partition table.


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


