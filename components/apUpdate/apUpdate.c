#include "apUpdate.h"

#include "esp_ota_ops.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

uint8_t otaInfoBytes[5] = {OTA_FIRMWARE_DEFAULT, OTA_AP_LAUNCHED, OTA_DEVICE_CONNECTED, OTA_FIRMWARE_UPLOADED, OTA_FIRMWARE_DONE};

// AP state
static esp_netif_t *ap_netif = NULL;
static esp_netif_t *sta_netif = NULL;
static esp_event_handler_instance_t wifi_event_handler_instance = NULL;

// Timeout management
static TaskHandle_t timeout_task_handle = NULL;
static bool ap_timeout_active = false;

#define AP_TIMEOUT_MINUTES 30
#define AP_TIMEOUT_MS (AP_TIMEOUT_MINUTES * 60 * 1000)

// Handle AP timeout
static void ap_timeout_task(void *pvParameters)
{
    ESP_LOGI("AP_TIMEOUT", "AP timeout task started. Will shutdown AP in %d minutes", AP_TIMEOUT_MINUTES);

    vTaskDelay(pdMS_TO_TICKS(AP_TIMEOUT_MS));

    if (ap_timeout_active)
    {
        ESP_LOGW("AP_TIMEOUT", "AP timeout reached (%d minutes). Automatically shutting down AP update mode", AP_TIMEOUT_MINUTES);
        stopAPUpdate();
        ap_timeout_active = false;
        timeout_task_handle = NULL;
    }

    vTaskDelete(NULL);
}

void apUpdateTask(void *pvParameters)
{
    startAP(CONFIG_SSID_SET);
    vTaskDelay(pdMS_TO_TICKS(1000));
    init_mdns(NULL); // Use default hostname from CONFIG_HOSTNAME_SET
    start_webserver();

    // NO TIMEOUT FOR NOW
    //   ap_timeout_active = true;
    //   BaseType_t xReturned = xTaskCreate(
    //       ap_timeout_task,
    //       "ap_timeout",
    //       4096,  // Stack size
    //       NULL,  // Parameters
    //       1,     // Priority
    //       &timeout_task_handle
    //   );
    //
    //   if (xReturned == pdPASS) {
    //       ESP_LOGI("AP_TIMEOUT", "Timeout task created successfully. AP will auto-shutdown in %d minutes", AP_TIMEOUT_MINUTES);
    //   } else {
    //       ESP_LOGE("AP_TIMEOUT", "Failed to create timeout task");
    //   }

    vTaskDelete(NULL);
}

void init_mdns(const char *hostname)
{
    static bool mdns_initialized = false;
    
    const char *mdns_hostname = hostname ? hostname : CONFIG_HOSTNAME_SET;

    if (mdns_initialized)
    {
        ESP_LOGI("MDNS", "mDNS already initialized, updating hostname to: %s", mdns_hostname);
        ESP_ERROR_CHECK(mdns_hostname_set(mdns_hostname));
        ESP_ERROR_CHECK(mdns_instance_name_set(mdns_hostname));
        return;
    }

    ESP_LOGI("MDNS", "Initializing mDNS with hostname: %s", mdns_hostname);
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(mdns_hostname));
    ESP_ERROR_CHECK(mdns_instance_name_set(mdns_hostname));

    mdns_txt_item_t serviceTxtData[1] = {
        {"path", "/"}
    };
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, serviceTxtData, 1));

    mdns_initialized = true;
    ESP_LOGI("MDNS", "mDNS service started successfully with hostname: %s.local", mdns_hostname);
}

void startAP(char *networkName)
{
    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_ERROR_CHECK(ret);
    }

    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_ERROR_CHECK(ret);
    }

    if (sta_netif == NULL)
    {
        sta_netif = esp_netif_create_default_wifi_sta();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_ERROR_CHECK(ret);
    }

    if (ap_netif == NULL)
    {
        ap_netif = esp_netif_create_default_wifi_ap();

        // Configure custom IP address for the AP
        esp_netif_ip_info_t ip_info;
        IP4_ADDR(&ip_info.ip, 10, 0, 0, 1);           // Custom IP: 10.0.0.1
        IP4_ADDR(&ip_info.gw, 10, 0, 0, 1);           // Gateway: 10.0.0.1
        IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0); // Subnet: 255.255.255.0

        esp_netif_dhcps_stop(ap_netif);
        ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info));
        esp_netif_dhcps_start(ap_netif);

        ESP_LOGI("wifiAP", "AP configured with custom IP: 10.0.0.1");
    }

    if (wifi_event_handler_instance == NULL)
    {
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &wifi_event_handler_instance);
    }

    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = strlen(networkName),
            .password = "",
            .max_connection = 1,
            .authmode = WIFI_AUTH_OPEN},
    };

    strcpy((char *)wifi_config.ap.ssid, networkName);
    strcpy((char *)wifi_config.ap.password, CONFIG_PASSWORD_SET);
    
    if (strlen(CONFIG_PASSWORD_SET) > 0) {
        wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("wifiAP", "Wi-Fi initialized in SoftAP mode");
}

bool validate_esp32_firmware(const uint8_t *data, size_t len)
{
    if (len < 32)
        return false;

    // Check for ESP32 firmware magic bytes (0xE9)
    if (data[0] != 0xE9)
    {
        ESP_LOGE("OTA", "Invalid firmware magic byte: 0x%02x", data[0]);
        return false;
    }

    ESP_LOGI("OTA", "Firmware header validation passed");
    return true;
}

esp_err_t ota_update_post_handler(httpd_req_t *req)
{
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    esp_err_t err = ESP_OK;
    bool ota_started = false;

    if (!ota_partition)
    {
        ESP_LOGE("OTA", "No OTA partition found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition available");
        return ESP_FAIL;
    }

    ESP_LOGI("OTA", "Starting OTA update. Running partition: %s, Target partition: %s",
             running_partition->label, ota_partition->label);

    // Begin OTA update
    err = esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE("OTA", "esp_ota_begin failed, error=%d", err);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA initialization failed");
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
            if (!validate_esp32_firmware((uint8_t *)buffer, received))
            {
                ESP_LOGE("OTA", "Invalid firmware format");
                if (ota_started)
                    esp_ota_abort(ota_handle);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid firmware format");
                return ESP_FAIL;
            }
            firmware_validated = true;

            // Log the first 10 bytes for debugging
            ESP_LOGI("OTA", "First 10 bytes: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                     buffer[0], buffer[1], buffer[2], buffer[3], buffer[4],
                     buffer[5], buffer[6], buffer[7], buffer[8], buffer[9]);

            first_chunk = false;
        }

        // Write to the OTA partition
        err = esp_ota_write(ota_handle, (const void *)buffer, received);
        if (err != ESP_OK)
        {
            ESP_LOGE("OTA", "OTA Write Failed at offset %d, error=%d", total_received, err);
            if (ota_started)
                esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Firmware write failed");
            return ESP_FAIL;
        }
        total_received += received;

        // Log progress every 64KB
        if (total_received % (64 * 1024) == 0)
        {
            ESP_LOGI("OTA", "Received %d bytes", total_received);
        }
    }

    if (received < 0)
    {
        ESP_LOGE("OTA", "File reception failed! Error: %d", received);
        if (ota_started)
            esp_ota_abort(ota_handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File reception failed");
        return ESP_FAIL;
    }

    if (total_received == 0)
    {
        ESP_LOGE("OTA", "No data received");
        if (ota_started)
            esp_ota_abort(ota_handle);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No firmware data received");
        return ESP_FAIL;
    }

    if (!firmware_validated)
    {
        ESP_LOGE("OTA", "Firmware validation failed");
        if (ota_started)
            esp_ota_abort(ota_handle);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Firmware validation failed");
        return ESP_FAIL;
    }

    ESP_LOGI("OTA", "Total firmware size received: %d bytes", total_received);

    // End OTA update
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE("OTA", "esp_ota_end failed, error=%d", err);

        // Check if this is a signature verification failure
        if (err == ESP_ERR_OTA_VALIDATE_FAILED)
        {
            ESP_LOGE("OTA", "Firmware signature verification failed - unauthorised firmware rejected");
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
        ESP_LOGE("OTA", "OTA set boot partition failed, error=%d", err);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Boot partition update failed");
        return ESP_FAIL;
    }

    ESP_LOGI("OTA", "Firmware update successful, rebooting...");
    httpd_resp_sendstr(req, "Firmware update successful. Rebooting...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI("wifiAP", "Device connected with MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                 event->mac[0], event->mac[1], event->mac[2], event->mac[3], event->mac[4], event->mac[5]);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI("wifiAP", "Device disconnected with MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                 event->mac[0], event->mac[1], event->mac[2], event->mac[3], event->mac[4], event->mac[5]);
    }
}

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t main_css_start[] asm("_binary_main_css_start");
extern const uint8_t main_css_end[] asm("_binary_main_css_end");
extern const uint8_t main_js_start[] asm("_binary_main_js_start");
extern const uint8_t main_js_end[] asm("_binary_main_js_end");

// GET handler to serve the HTML page
esp_err_t get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    const size_t html_size = index_html_end - index_html_start;
    httpd_resp_send(req, (const char *)index_html_start, html_size);
    return ESP_OK;
}

esp_err_t css_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/css");
    const size_t css_size = main_css_end - main_css_start;
    httpd_resp_send(req, (const char *)main_css_start, css_size);
    return ESP_OK;
}

esp_err_t js_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    const size_t js_size = main_js_end - main_js_start;
    httpd_resp_send(req, (const char *)main_js_start, js_size);
    return ESP_OK;
}

esp_err_t redirect_handler(httpd_req_t *req)
{
    // Redirect all requests to the root page with custom IP
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://10.0.0.1/");
    httpd_resp_send(req, NULL, 0); // Response body can be empty
    return ESP_OK;
}

static httpd_handle_t server = NULL;

void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.stack_size = 8192;
    config.task_priority = 5;
    config.max_uri_handlers = 10;
    config.max_resp_headers = 8;

    ESP_ERROR_CHECK(httpd_start(&server, &config));

    httpd_uri_t uri_get = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = get_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &uri_get);

    httpd_uri_t uri_css = {
        .uri = "/main.css",
        .method = HTTP_GET,
        .handler = css_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &uri_css);

    httpd_uri_t uri_js = {
        .uri = "/main.js",
        .method = HTTP_GET,
        .handler = js_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &uri_js);

    httpd_uri_t uri_ota_update = {
        .uri = "/ota_update",
        .method = HTTP_POST,
        .handler = ota_update_post_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &uri_ota_update);

    // Catch-all
    httpd_uri_t uri_redirect = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = redirect_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &uri_redirect);
}

void stop_webserver(void)
{
    if (server != NULL)
    {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI("HTTP_SERVER", "Web server stopped");
    }
}

void deinit_ap_mdns(void)
{
    mdns_free();
    ESP_LOGI("MDNS", "mDNS deinitialised");
}

void deinit_ap_wifi(void)
{
    ESP_ERROR_CHECK(esp_wifi_stop());

    if (wifi_event_handler_instance != NULL)
    {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler_instance);
        wifi_event_handler_instance = NULL;
    }

    if (ap_netif != NULL)
    {
        esp_netif_destroy_default_wifi(ap_netif);
        ap_netif = NULL;
    }

    if (sta_netif != NULL)
    {
        esp_netif_destroy_default_wifi(sta_netif);
        sta_netif = NULL;
    }

    ESP_LOGI("WIFI", "Wi-Fi stopped and cleaned up");
}

void stopAPUpdate(void)
{
    if (timeout_task_handle != NULL && ap_timeout_active)
    {
        ESP_LOGI("AP_TIMEOUT", "Cancelling AP timeout task");
        ap_timeout_active = false;
        vTaskDelete(timeout_task_handle);
        timeout_task_handle = NULL;
    }

    stop_webserver();

    deinit_ap_mdns();
    deinit_ap_wifi();

    ESP_LOGI("AP_UPDATE", "All AP Update components deinitialized");
}

// Timeout helpers
void cancel_ap_timeout(void)
{
    if (timeout_task_handle != NULL && ap_timeout_active)
    {
        ESP_LOGI("AP_TIMEOUT", "Manually cancelling AP timeout");
        ap_timeout_active = false;
        vTaskDelete(timeout_task_handle);
        timeout_task_handle = NULL;
    }
}

bool is_ap_timeout_active(void)
{
    return ap_timeout_active;
}
