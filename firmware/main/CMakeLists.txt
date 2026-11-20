idf_component_register(SRCS "app_main.c" "app_fsm.c"
                       INCLUDE_DIRS "."
                       REQUIRES wifi_mgr scanner api motor ui nvs_flash esp_timer)
