#include "otaHandler.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "OTA_HANDLER";

static bool diagnostic(void) 
{
    bool diagnostic_is_ok = 1; // Needs to be customised for the specific use case
    return diagnostic_is_ok;
}

bool otaHandler_validateFirmware(const uint8_t *data, size_t len)
{
    if (len < 32)
        return false;

    // Check for ESP32 firmware magic bytes (0xE9)
    if (data[0] != 0xE9)
    {
        ESP_LOGE(TAG, "Invalid firmware magic byte: 0x%02x", data[0]);
        return false;
    }

    ESP_LOGI(TAG, "Firmware header validation passed");
    return true;
}

void otaHandler_validateUpdate(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    ESP_LOGI(TAG, "Starting OTA validation check...");
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
        {
        ESP_LOGI(TAG, "Running partition: %s, address: 0x%08lX", running->label, (unsigned long)running->address);
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

esp_err_t otaHandler_updatePostHandler(httpd_req_t *req)
{
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    esp_err_t err = ESP_OK;
    bool ota_started = false;

    if (!ota_partition)
    {
        ESP_LOGE(TAG, "No OTA partition found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition available");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Starting OTA update. Running partition: %s, Target partition: %s",
             running_partition->label, ota_partition->label);

    // Begin OTA update
    err = esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_begin failed, error=%d", err);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA initialisation failed");
        return ESP_FAIL;
    }
    ota_started = true;

    char buffer[512];
    int received;
    int total_received = 0;
    bool first_chunk = true;
    bool firmware_validated = false;

    // Receive firmware data in chunks
    while ((received = httpd_req_recv(req, buffer, sizeof(buffer))) > 0)
    {
        if (first_chunk)
        {
            // Validate firmware header on first chunk
            if (!otaHandler_validateFirmware((uint8_t *)buffer, received))
            {
                ESP_LOGE(TAG, "Invalid firmware format");
                if (ota_started)
                    esp_ota_abort(ota_handle);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid firmware format");
                return ESP_FAIL;
            }
            firmware_validated = true;

            // Log the first 10 bytes for debugging
            ESP_LOGI(TAG, "First 10 bytes: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                     buffer[0], buffer[1], buffer[2], buffer[3], buffer[4],
                     buffer[5], buffer[6], buffer[7], buffer[8], buffer[9]);

            first_chunk = false;
        }

        // Write to the OTA partition
        err = esp_ota_write(ota_handle, (const void *)buffer, received);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "OTA Write Failed at offset %d, error=%d", total_received, err);
            if (ota_started)
                esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Firmware write failed");
            return ESP_FAIL;
        }
        total_received += received;

        // Log progress every 64KB
        if (total_received % (64 * 1024) == 0)
        {
            ESP_LOGI(TAG, "Received %d bytes", total_received);
        }
    }

    if (received < 0)
    {
        ESP_LOGE(TAG, "File reception failed! Error: %d", received);
        if (ota_started)
            esp_ota_abort(ota_handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File reception failed");
        return ESP_FAIL;
    }

    if (total_received == 0)
    {
        ESP_LOGE(TAG, "No data received");
        if (ota_started)
            esp_ota_abort(ota_handle);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No firmware data received");
        return ESP_FAIL;
    }

    if (!firmware_validated)
    {
        ESP_LOGE(TAG, "Firmware validation failed");
        if (ota_started)
            esp_ota_abort(ota_handle);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Firmware validation failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Total firmware size received: %d bytes", total_received);

    // End OTA update
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_end failed, error=%d", err);

        // Check if this is a signature verification failure
        if (err == ESP_ERR_OTA_VALIDATE_FAILED)
        {
            ESP_LOGE(TAG, "Firmware signature verification failed - unauthorised firmware rejected");
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                "Firmware verification failed: This device requires signed firmware. "
                                "Please upload a firmware file that has been properly signed with the authorised key.");
        }
        else
        {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA finalisation failed");
        }
        return ESP_FAIL;
    }
    ota_started = false;

    // Set OTA partition as boot partition
    err = esp_ota_set_boot_partition(ota_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "OTA set boot partition failed, error=%d", err);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Boot partition update failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Firmware update successful, rebooting...");
    httpd_resp_sendstr(req, "Firmware update successful. Rebooting...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}