idf_component_register(SRCS "hid_keymap.c" "scanner_line.c" "scanner_devs.c" "scanner.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_timer usb)
