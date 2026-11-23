idf_component_register(SRCS "ui.c" "ui_board.c" "ui_image.c" "ui_debug.c" "ui_input.c" "ui_scan.c"
                            "fonts/ui_font_tc_24.c" "fonts/ui_font_tc_32.c" "fonts/ui_font_tc_56.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "."
                       EMBED_FILES "assets/standby.jpg"
                       REQUIRES esp_lcd esp_driver_i2c esp_driver_ledc esp_driver_gpio esp_hw_support
                                esp_driver_jpeg esp_mm hal standby_store)
