# AP OTA Update Component

## Features
- Sets up an Access Point (AP) on the ESP32 with a configurable SSID and password
- Serves a simple HTML page for uploading a firmware `.bin` file via a web interface
- Performs OTA updates using the uploaded firmware and reboots the ESP32 after a successful update
- Configures mDNS service for easier access to the web interface

## Configuration

### Kconfig Options

The following options can be configured in menuconfig `AP OTA Update Config`:

- **HOSTNAME_SET (string)**: Hostname for the mDNS service. Default: `Simple OTA`
- **SSID_SET (string)**: SSID for the AP. Default: `Simple OTA`
- **PASSWORD_SET (string)**: Password for the AP. Default: `simpleOTA`

## Usage

1. **Initialize the AP and Webserver**:

   ```c
   xTaskCreate(&apUpdateTask, "apUpdateTask", 4096, NULL, 5, NULL);

## Functions

### `apUpdateTask(void *pvParameters)`
This function starts the AP and web server for OTA updates. It launches the AP with the configured SSID and password and serves the firmware upload page.

### `startAP(void)`
Initializes the ESP32 in SoftAP mode, sets the AP SSID and password, and configures Wi-Fi settings. It also initializes the mDNS service and starts broadcasting the device’s hostname.

### `init_mdns(void)`
Initializes the mDNS service, setting the hostname and instance name, and adds an HTTP service to the mDNS. This allows the AP to be accessible using the hostname instead of the IP address.

### `ota_update_post_handler(httpd_req_t *req)`
Handles the HTTP POST request for uploading the OTA firmware. This function receives the firmware file in chunks, writes it to the OTA partition, and sets the new partition as the boot partition.

### `start_webserver(void)`
Starts an HTTP server that listens for firmware uploads. It registers handlers for the root page (GET request) and the OTA update endpoint (POST request). The web server also redirects any unspecified URLs back to the root page.

