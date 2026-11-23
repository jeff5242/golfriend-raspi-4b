idf_component_register(SRCS "camera.c" "qr_code.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_i2c
                       PRIV_REQUIRES esp_timer ov02c10)
