#include "OTA.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static const char *TAG = "OTA";

static bool diagnostic(void) 
{
    bool diagnostic_is_ok = 1; // Needs to be customised for the specific use case
    return diagnostic_is_ok;
}

void validateOTAupdate(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    ESP_LOGI("OTA", "Starting OTA validation check...");
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
        {
        ESP_LOGI("OTA", "Running partition: %s, address: 0x%08lX", running->label, (unsigned long)running->address);
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
        {
            ESP_LOGI(TAG, "OTA state pending verify - running diagnostics...");
            bool diagnostic_is_ok = diagnostic();
            if (diagnostic_is_ok)
            {
                ESP_LOGI(TAG, "Diagnostics completed successfully! Continuing execution ...");
                esp_ota_mark_app_valid_cancel_rollback();
            }
            else
            {
                ESP_LOGE(TAG, "Diagnostics failed! Start rollback to the previous version ...");
                esp_ota_mark_app_invalid_rollback_and_reboot();
            }
        }
        else
        {
            ESP_LOGI(TAG, "OTA state is %d, no action required.", ota_state);
        }
    }
    else{
        ESP_LOGI(TAG, "Normal execution ...");
    }
}