idf_component_register(SRCS "ina219.c"
                    INCLUDE_DIRS "include"
                    REQUIRES i2c esp_timer sd_card)