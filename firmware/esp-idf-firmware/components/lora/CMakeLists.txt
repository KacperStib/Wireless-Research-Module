idf_component_register(SRCS "lora.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver esp_driver_spi esp_driver_gpio dev_config radio)

