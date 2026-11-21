#pragma once
/* 主機端測試用的 ESP-IDF 替身：只放 motor.c 用到的部分。 */
typedef int esp_err_t;
#define ESP_OK                0
#define ESP_FAIL              -1
#define ESP_ERR_NO_MEM        0x101
#define ESP_ERR_INVALID_ARG   0x102
#define ESP_ERR_INVALID_STATE 0x103
const char *esp_err_to_name(esp_err_t code);
