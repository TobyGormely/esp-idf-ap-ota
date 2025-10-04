#include "apUpdate.h"
#include "OTA.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP OTA Application");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    validateOTAupdate();

    ESP_LOGI(TAG, "Starting AP update mode...");
    xTaskCreate(apUpdateTask, "apUpdateTask", 4096, NULL, 5, NULL);

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
