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

extern uint8_t otaInfoBytes[5];
bool validate_esp32_firmware(const uint8_t* data, size_t len);
void startAP(char *networkName);
void apUpdateTask(void *pvParameters);
void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void start_webserver(void);
void stopAPUpdate(void);
void init_mdns(const char* hostname);

// Timeout control functions
void cancel_ap_timeout(void);
bool is_ap_timeout_active(void);

#endif // APUPDATE_H 


