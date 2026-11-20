idf_component_register(SRCS "app_main.c" "app_fsm.c" "app_dispense.c"
                       INCLUDE_DIRS "."
                       REQUIRES wifi_mgr scanner api motor ui nvs_flash esp_timer)
