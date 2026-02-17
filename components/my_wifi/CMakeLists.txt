idf_component_register(SRCS "my_wifi.c"
                    INCLUDE_DIRS "include"
                    REQUIRES my_led nvs_flash esp_wifi esp_event esp_netif esp_timer esp_http_server esp_http_client)
