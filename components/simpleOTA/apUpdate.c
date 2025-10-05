#include "apUpdate.h"
#include "otaHandler.h"

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
        apUpdate_stop();
        ap_timeout_active = false;
        timeout_task_handle = NULL;
    }

    vTaskDelete(NULL);
}

void apUpdate_task(void *pvParameters)
{
    apUpdate_startAP(CONFIG_SIMPLE_OTA_AP_SSID);
    vTaskDelay(pdMS_TO_TICKS(1000));
    apUpdate_initMdns(CONFIG_SIMPLE_OTA_HOSTNAME);
    apUpdate_startWebserver();

    ap_timeout_active = true;
    BaseType_t xReturned = xTaskCreate(
        ap_timeout_task,
        "ap_timeout",
        4096, // Stack size
        NULL, // Parameters
        1,    // Priority
        &timeout_task_handle);

    if (xReturned == pdPASS)
    {
        ESP_LOGI("AP_TIMEOUT", "Timeout task created successfully. AP will auto-shutdown in %d minutes", AP_TIMEOUT_MINUTES);
    }
    else
    {
        ESP_LOGE("AP_TIMEOUT", "Failed to create timeout task");
    }

    vTaskDelete(NULL);
}

void apUpdate_initMdns(const char *hostname)
{
    static bool mdns_initialised = false;

    const char *mdns_hostname = hostname ? hostname : CONFIG_SIMPLE_OTA_HOSTNAME;

    if (mdns_initialised)
    {
        ESP_LOGI("MDNS", "mDNS already initialised, updating hostname to: %s", mdns_hostname);
        ESP_ERROR_CHECK(mdns_hostname_set(mdns_hostname));
        ESP_ERROR_CHECK(mdns_instance_name_set(mdns_hostname));
        return;
    }

    ESP_LOGI("MDNS", "Initialising mDNS with hostname: %s", mdns_hostname);
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(mdns_hostname));
    ESP_ERROR_CHECK(mdns_instance_name_set(mdns_hostname));

    mdns_txt_item_t serviceTxtData[1] = {
        {"path", "/"}};
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, serviceTxtData, 1));

    mdns_initialised = true;
    ESP_LOGI("MDNS", "mDNS service started successfully with hostname: %s.local", mdns_hostname);
}

void apUpdate_startAP(char *networkName)
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
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &apUpdate_wifiEventHandler, NULL, &wifi_event_handler_instance);
    }

    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = strlen(networkName),
            .password = "",
            .max_connection = 1,
            .authmode = WIFI_AUTH_OPEN},
    };

    strcpy((char *)wifi_config.ap.ssid, networkName);
    strcpy((char *)wifi_config.ap.password, CONFIG_SIMPLE_OTA_AP_PASSWORD);

    if (strlen(CONFIG_SIMPLE_OTA_AP_PASSWORD) > 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("wifiAP", "Wi-Fi initialised in SoftAP mode");
}

void apUpdate_wifiEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
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
    
    // First send the configuration variable
    char config_js[128];
    snprintf(config_js, sizeof(config_js), 
             "const CONFIG_MAX_FILE_SIZE_MB = %d;\n", 
             CONFIG_SIMPLE_OTA_MAX_FILE_SIZE_MB);
    httpd_resp_send_chunk(req, config_js, strlen(config_js));
    
    // Then send the main JavaScript file
    const size_t js_size = main_js_end - main_js_start;
    httpd_resp_send_chunk(req, (const char *)main_js_start, js_size);
    
    // End the chunked response
    httpd_resp_send_chunk(req, NULL, 0);
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

void apUpdate_startWebserver(void)
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
        .handler = otaHandler_updatePostHandler,
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

void apUpdate_stop(void)
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

    ESP_LOGI("AP_UPDATE", "All AP Update components deinitialised");
}

// Timeout helpers
void apUpdate_cancelTimeout(void)
{
    if (timeout_task_handle != NULL && ap_timeout_active)
    {
        ESP_LOGI("AP_TIMEOUT", "Manually cancelling AP timeout");
        ap_timeout_active = false;
        vTaskDelete(timeout_task_handle);
        timeout_task_handle = NULL;
    }
}

bool apUpdate_isTimeoutActive(void)
{
    return ap_timeout_active;
}
