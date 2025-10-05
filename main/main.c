#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "simpleOTA.h"

static const char* TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP OTA Application");

    // Initialise NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Validate any pending OTA updates
    simpleOTA_validateOnBoot();

    // Start OTA with kconfig settings
    ESP_ERROR_CHECK(simpleOTA_start());

    ESP_LOGI(TAG, "OTA Ready! Connect to 'Simple OTA' AP and visit simpleOTA.local");

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}