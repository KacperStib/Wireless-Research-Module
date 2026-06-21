idf_component_register(SRCS "shell_mng.c"
                    INCLUDE_DIRS "include"
                    REQUIRES console dev_config esp_now_c lora nvs_storage)

