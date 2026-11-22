idf_component_register(SRCS "tfcard.c"
                       INCLUDE_DIRS "include"
                       REQUIRES fatfs esp_driver_sdmmc sdmmc)
