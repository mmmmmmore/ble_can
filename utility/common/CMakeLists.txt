idf_component_register(SRCS "security_utils.c" "storage_utils.c"
                    INCLUDE_DIRS "include"
                    REQUIRES mbedtls spiffs uds)
