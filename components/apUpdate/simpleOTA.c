#include "simpleOTA.h"
#include "apUpdate.h"
#include "otaHandler.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "SimpleOTA";
static simple_ota_config_t current_config;
static simple_ota_status_t current_status = SIMPLE_OTA_IDLE;
static simple_ota_event_cb_t event_callback = NULL;
static bool ota_initialised = false;

// Internal task to manage OTA lifecycle
static void simple_ota_task(void* pvParameters)
{
    simple_ota_config_t* config = (simple_ota_config_t*)pvParameters;
    
    ESP_LOGI(TAG, "Starting Simple OTA with SSID: %s", config->ap_ssid);
    
    // Update status
    current_status = SIMPLE_OTA_AP_STARTED;
    if (event_callback) {
        event_callback(current_status, 0, "Access Point started");
    }
    
    apUpdate_task(NULL);
    
    while (current_status != SIMPLE_OTA_IDLE) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    vTaskDelete(NULL);
}

esp_err_t simpleOTA_start(void)
{
    simple_ota_config_t config = SIMPLE_OTA_DEFAULT_CONFIG();
    return simpleOTA_startWithConfig(&config);
}

// Pass config into OTA start at runtime
esp_err_t simpleOTA_startWithConfig(const simple_ota_config_t* config)
{
    if (ota_initialised) {
        ESP_LOGW(TAG, "Simple OTA already running");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!config) {
        ESP_LOGE(TAG, "Configuration cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Copy configuration
    current_config = *config;
    ota_initialised = true;
    current_status = SIMPLE_OTA_IDLE;
    
    ESP_LOGI(TAG, "Initialising Simple OTA Library");
    ESP_LOGI(TAG, "AP SSID: %s", current_config.ap_ssid);
    ESP_LOGI(TAG, "Hostname: %s.local", current_config.hostname);
    ESP_LOGI(TAG, "Timeout: %d minutes", current_config.timeout_minutes);
    
    // Create OTA task
    BaseType_t result = xTaskCreate(
        simple_ota_task,
        "simple_ota_task",
        8192,  // Stack size
        &current_config,
        5,     // Priority
        NULL
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Simple OTA task");
        ota_initialised = false;
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

esp_err_t simpleOTA_stop(void)
{
    if (!ota_initialised) {
        ESP_LOGW(TAG, "Simple OTA not running");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Stopping Simple OTA");
    
    // Stop the AP update service
    apUpdate_stop();
    
    current_status = SIMPLE_OTA_IDLE;
    ota_initialised = false;
    
    if (event_callback) {
        event_callback(current_status, 0, "OTA service stopped");
    }
    
    return ESP_OK;
}

esp_err_t simpleOTA_setCallback(simple_ota_event_cb_t callback)
{
    event_callback = callback;
    return ESP_OK;
}

simple_ota_status_t simpleOTA_getStatus(void)
{
    return current_status;
}

bool simpleOTA_isRunning(void)
{
    return ota_initialised && (current_status != SIMPLE_OTA_IDLE);
}

const char* simpleOTA_getApIp(void)
{
    if (current_status == SIMPLE_OTA_AP_STARTED || 
        current_status == SIMPLE_OTA_CLIENT_CONNECTED ||
        current_status == SIMPLE_OTA_UPLOADING) {
        return "10.0.0.1";  // Standard AP IP
    }
    return NULL;
}

esp_err_t simpleOTA_validateOnBoot(void)
{
    ESP_LOGI(TAG, "Validating OTA update on boot");
    otaHandler_validateUpdate();
    return ESP_OK;
}