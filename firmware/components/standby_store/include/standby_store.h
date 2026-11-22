#pragma once
/*
 * 各場館的待機圖：存在 Flash 的 "standby" 分割區，不必重新編譯韌體就能換圖。
 * 來源是 TF 卡上的 standby.jpg（開機時匯入）。分割區沒有圖時，畫面用編進韌體的預設圖。
 *
 * 圖片規格：1024x600、baseline JPEG（不是 progressive）、檔案小於分割區大小。
 */
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 把 path 的檔案寫進分割區。和已存的圖相同時不寫（*changed = false），避免每次開機磨損 Flash。
 * 檔案不存在回傳 ESP_ERR_NOT_FOUND；太大回傳 ESP_ERR_INVALID_SIZE。
 */
esp_err_t standby_store_import_file(const char *path, bool *changed);

/*
 * 取得已存的 JPEG（記憶體映射，不佔 RAM；指標在程式執行期間一直有效）。
 * 沒有圖或 CRC 不符時回傳錯誤。
 */
esp_err_t standby_store_get(const uint8_t **jpg, uint32_t *size);

#ifdef __cplusplus
}
#endif
