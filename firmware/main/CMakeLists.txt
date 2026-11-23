idf_component_register(SRCS "app_main.c" "app_fsm.c" "app_dispense.c" "app_typed.c" "app_net.c"
                       INCLUDE_DIRS "."
                       REQUIRES wifi_mgr eth_mgr settings remote_log ota tfcard standby_store scanner api motor ui nvs_flash esp_timer esp_netif esp_app_format)
