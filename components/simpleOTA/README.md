# ESP32 Simple OTA Component

Easy-to-use OTA (Over-The-Air) update library for ESP32 projects.

## Features

- **Simple API** - Get OTA running with one function call
- **Web Interface** - Clean web UI for firmware uploads  
- **Kconfig Integration** - Configure via menuconfig
- **Secure** - Firmware validation and rollback support

## Quick Start

### 1. Add to your project

**Method A: Copy component**
```bash
cp -r apUpdate your-project/components/
```

**Method B: Git submodule**
```bash
cd your-project/components
git submodule add https://github.com/TobyGormely/esp-idf-ap-ota.git apUpdate
```

### 2. Use in your code

```c
#include "simpleOTA.h"

void app_main(void) {
    // Initialise NVS
    nvs_flash_init();
    
    // Validate any pending OTA updates
    simpleOTA_validateOnBoot();
    
    // Start OTA with default settings
    simpleOTA_start();
    
    // Your app code here...
}
```

### 3. Configure (optional)

Run `idf.py menuconfig` → **Simple OTA Configuration** to customise:
- WiFi AP name and password
- mDNS hostname  
- Timeout settings

## Usage

Connect to the device's WiFi network and visit your set hostname (.local) or http://10.0.0.1 to upload firmware.

## API Reference

| Function | Description |
|----------|-------------|
| `simpleOTA_start()` | Start with Kconfig defaults |
| `simpleOTA_startWithConfig()` | Start with custom config |
| `simpleOTA_validateOnBoot()` | Validate OTA on startup |
| `simpleOTA_stop()` | Stop OTA service |

## License

MIT License - see LICENSE file for details.