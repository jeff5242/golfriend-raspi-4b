idf_component_register(SRCS "api_sign.c" "api_errors.c" "api.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_http_client esp_netif json mbedtls)
