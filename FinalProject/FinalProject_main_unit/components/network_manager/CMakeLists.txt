idf_component_register(SRCS "network_manager.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_netif esp_http_server nvs_flash sensor_manager)

