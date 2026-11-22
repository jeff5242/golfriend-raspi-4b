#pragma once

#include "esp_err.h"
#include "lvgl.h"

/*
 * 把編進韌體的待機圖（assets/standby.jpg，1024x600）用硬體 JPEG 解碼成 RGB565。
 * 只解一次，結果放在 PSRAM，整個執行期間都有效。失敗時 *out 不變。
 */
esp_err_t ui_image_load_standby(const lv_image_dsc_t **out);
