idf_component_register(SRCS "ui.c"
                       INCLUDE_DIRS "include")
# 接上 BSP 時加：REQUIRES esp_lcd，並在 idf_component.yml 打開 lvgl 與 esp_lvgl_port
