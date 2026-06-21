idf_component_register(SRCS "esp_now_c.c"
                    INCLUDE_DIRS "include"
                    REQUIRES nvs_flash esp_event esp_netif esp_wifi dev_config sd_card gps radio)

