idf_component_register(SRCS "ota_policy.c" "ota.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_https_ota esp_http_client app_update json esp-tls mbedtls)
