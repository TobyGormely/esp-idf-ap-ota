#ifndef OTA_HANDLER_H
#define OTA_HANDLER_H

#include "esp_ota_ops.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include <stdio.h>
#include <stdbool.h>

// OTA validation and rollback
void otaHandler_validateUpdate(void);

// Firmware validation
bool otaHandler_validateFirmware(const uint8_t *data, size_t len);

// OTA upload handler for HTTP server
esp_err_t otaHandler_updatePostHandler(httpd_req_t *req);

#endif // OTA_HANDLER_H