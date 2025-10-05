idf_component_register(SRCS "simpleOTA.c" "apUpdate.c" "otaHandler.c"
                       INCLUDE_DIRS "include"
                       REQUIRES  "esp_wifi" "esp_https_server" "espressif__mdns" "app_update" "driver")
