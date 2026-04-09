idf_component_register(SRCS "uds_protocol.c" "uds_services.c" "uds_security.c"
                    INCLUDE_DIRS "include"
                    REQUIRES can mbedtls)
