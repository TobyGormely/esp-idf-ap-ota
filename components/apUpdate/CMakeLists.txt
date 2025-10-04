idf_component_register(SRCS "apUpdate.c" "OTA.c"
                       INCLUDE_DIRS "include"
                       REQUIRES  "esp_wifi" "esp_https_server" "espressif__mdns" "app_update" "driver")
