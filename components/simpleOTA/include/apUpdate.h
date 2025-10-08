#ifndef APUPDATE_H
#define APUPDATE_H

#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/ip4_addr.h"
#include "mdns.h"

#define OTA_FIRMWARE_DEFAULT 0
#define OTA_AP_LAUNCHED 1
#define OTA_DEVICE_CONNECTED 2
#define OTA_FIRMWARE_UPLOADED 3
#define OTA_FIRMWARE_DONE 4

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t main_css_start[] asm("_binary_main_css_start");
extern const uint8_t main_css_end[] asm("_binary_main_css_end");
extern const uint8_t main_js_start[] asm("_binary_main_js_start");
extern const uint8_t main_js_end[] asm("_binary_main_js_end");

void apUpdate_startAP(char *networkName);
void apUpdate_task(void *pvParameters);
void apUpdate_wifiEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void apUpdate_startWebserver(void);
void apUpdate_stop(void);
void apUpdate_initMdns(const char* hostname);

// Timeout control functions
void apUpdate_cancelTimeout(void);
bool apUpdate_isTimeoutActive(void);

#endif // APUPDATE_H 


