#pragma once
/*
 * 韌體遠端更新（OTA）。
 *
 * 流程：讀「版本說明檔」（JSON）→ 版本比目前新 → 下載到另一個 app 分割區 → 重開機。
 * 版本說明檔：{"version":"P4-0.1.1","url":"https://.../golfriend_p4.bin"}
 *
 * 退回機制（bootloader 的 app rollback）：新版第一次開機是「待確認」。
 * 呼叫 ota_confirm_running() 之前如果重開機，bootloader 會退回上一版。
 * 所以必須在確定新版能正常連到後台之後才呼叫。
 */
#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 讀 manifest_url，判斷有沒有新版。有新版時回傳 ESP_OK，下載網址寫到 url、版本寫到 version。
 * 沒有新版回傳 ESP_ERR_NOT_FOUND。其他錯誤：網路、格式、網址不允許。
 */
esp_err_t ota_check(const char *manifest_url, char *url, size_t url_size, char *version,
                    size_t version_size);

/* 下載 url 的韌體並設為下次開機的版本。成功後呼叫端必須重開機。最多花數分鐘。 */
esp_err_t ota_download(const char *url);

/* 目前執行的版本如果是「待確認」，標記為正常（取消退回）。可以重複呼叫。 */
void ota_confirm_running(void);

/* 目前執行的版本是否還在「待確認」。 */
bool ota_is_pending_verify(void);

#ifdef __cplusplus
}
#endif
