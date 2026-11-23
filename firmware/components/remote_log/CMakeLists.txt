idf_component_register(SRCS "remote_log_format.c" "remote_log.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_http_client esp-tls mbedtls)
