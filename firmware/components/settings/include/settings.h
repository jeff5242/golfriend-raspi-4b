#pragma once
/*
 * 現場設定（裝置代碼、WiFi）。來源依優先順序：
 *   1. NVS（由 TF 卡的 config.txt 匯入後存進板子）
 *   2. menuconfig 的預設值（開發用；出貨韌體可以留空）
 * 呼叫前必須已經 nvs_flash_init()。
 */
#include "esp_err.h"
#include "settings_parse.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 讀出目前的設定。NVS 沒有的項目用 menuconfig 的值。 */
void settings_load(settings_t *out);

/* 把設定存進 NVS（整組一次寫入）。 */
esp_err_t settings_save(const settings_t *settings);

/*
 * 從設定檔匯入：讀 path（例如 TF 卡的 config.txt），套用到 *settings 並存進 NVS。
 * 回傳套用的項目數；檔案不存在、讀不到、或沒有任何有效項目時回傳 0，*settings 不變。
 */
int settings_import_file(const char *path, settings_t *settings);

#ifdef __cplusplus
}
#endif
