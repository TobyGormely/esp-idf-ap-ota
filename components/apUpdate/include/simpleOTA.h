#ifndef SIMPLE_OTA_H
#define SIMPLE_OTA_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Simple OTA Configuration Structure
 */
typedef struct {
    const char* ap_ssid;        ///< Access Point SSID (default: "Simple OTA")
    const char* ap_password;    ///< Access Point password (default: "simpleOTA")
    const char* hostname;       ///< mDNS hostname (default: "simpleota")
    uint16_t timeout_minutes;   ///< AP timeout in minutes (default: 0, 0 = no timeout)
    bool auto_reboot;           ///< Auto reboot after successful OTA (default: true)
} simple_ota_config_t;

/**
 * @brief OTA Update Status
 */
typedef enum {
    SIMPLE_OTA_IDLE,
    SIMPLE_OTA_AP_STARTED,
    SIMPLE_OTA_CLIENT_CONNECTED,
    SIMPLE_OTA_UPLOADING,
    SIMPLE_OTA_SUCCESS,
    SIMPLE_OTA_FAILED,
    SIMPLE_OTA_TIMEOUT
} simple_ota_status_t;

/**
 * @brief OTA Event callback function type
 * 
 * @param status Current OTA status
 * @param progress Upload progress (0-100) when status is SIMPLE_OTA_UPLOADING
 * @param message Optional status message
 */
typedef void (*simple_ota_event_cb_t)(simple_ota_status_t status, int progress, const char* message);

/**
 * @brief Default configuration initialiser (uses Kconfig values)
 * 
 * Uses values from menuconfig -> Simple OTA Configuration
 * Run 'idf.py menuconfig' to customise default settings.
 * 
 * @return Default simple_ota_config_t structure from Kconfig
 */
#define SIMPLE_OTA_DEFAULT_CONFIG() { \
    .ap_ssid = CONFIG_SIMPLE_OTA_AP_SSID, \
    .ap_password = CONFIG_SIMPLE_OTA_AP_PASSWORD, \
    .hostname = CONFIG_SIMPLE_OTA_HOSTNAME, \
    .timeout_minutes = CONFIG_SIMPLE_OTA_TIMEOUT_MINUTES, \
    .auto_reboot = CONFIG_SIMPLE_OTA_AUTO_REBOOT \
}

/**
 * @brief Initialise Simple OTA with default configuration from Kconfig
 * 
 * Uses settings from 'idf.py menuconfig' -> Simple OTA Configuration.
 * This is the recommended way for most users - configure once in menuconfig,
 * then just call simpleOTA_start() in your code.
 * 
 * @return ESP_OK on success
 */
esp_err_t simpleOTA_start(void);

/**
 * @brief Initialise Simple OTA with runtime configuration
 * 
 * Use this when you need to override Kconfig defaults at runtime
 * (e.g., based on user settings, device MAC address, etc.)
 * 
 * @param config Configuration structure
 * @return ESP_OK on success
 */
esp_err_t simpleOTA_startWithConfig(const simple_ota_config_t* config);

/**
 * @brief Stop OTA service
 * 
 * @return ESP_OK on success
 */
esp_err_t simpleOTA_stop(void);

/**
 * @brief Set event callback for OTA status updates
 * 
 * @param callback Callback function to receive status updates
 * @return ESP_OK on success
 */
esp_err_t simpleOTA_setCallback(simple_ota_event_cb_t callback);

/**
 * @brief Get current OTA status
 * 
 * @return Current simple_ota_status_t
 */
simple_ota_status_t simpleOTA_getStatus(void);

/**
 * @brief Check if OTA is currently running
 * 
 * @return true if OTA service is active
 */
bool simpleOTA_isRunning(void);

/**
 * @brief Get the AP IP address (usually 10.0.0.1)
 * 
 * @return IP address string, or NULL if AP not started
 */
const char* simpleOTA_getApIp(void);

/**
 * @brief Validate OTA update on boot (call this in app_main)
 * 
 * This should be called early in app_main() to confirm successful OTA updates
 * or rollback to previous firmware if the new firmware fails.
 * 
 * @return ESP_OK if validation successful
 */
esp_err_t simpleOTA_validateOnBoot(void);

#ifdef __cplusplus
}
#endif

#endif // SIMPLE_OTA_H