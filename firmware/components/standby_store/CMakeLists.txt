idf_component_register(SRCS "standby_header.c" "standby_store.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_partition esp_rom)
