#pragma once
/*
 * OTA 的判斷規則（純邏輯，可以在主機端測試）。
 * 版本格式："<機型>-<主>.<次>.<修>"，例如 "P4-0.1.0"（firmware/CMakeLists.txt 的 PROJECT_VER）。
 */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * candidate 是否比 current 新。下列情況都回傳 false（不更新）：
 *   - 任一個版本字串格式不對
 *   - 機型不同（例如 P4 的板子拿到別種板子的韌體）
 *   - 相同或較舊（不降版，避免兩個版本互相更新）
 */
bool ota_is_newer(const char *current, const char *candidate);

/* 下載網址是否允許：必須是 https://；allow_http 為 true 時也接受 http://（只給開發用）。 */
bool ota_url_allowed(const char *url, bool allow_http);

#ifdef __cplusplus
}
#endif
