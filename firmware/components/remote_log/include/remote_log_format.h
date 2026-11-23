#pragma once
/*
 * 遠端 log 的 JSON 格式（純邏輯，可以在主機端測試）。欄位比照 Pi 版 log.py 的 LogtailHandler：
 *   {"dt":"2026-11-23T01:20:00+0800","level":"INFO","message":"[裝置] [標籤] 內容",
 *    "device":"裝置","tag":"system"}
 * 一次送出的內容是這種物件的陣列。
 */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    REMOTE_LOG_SYSTEM = 0,  /* 開機、網路、設定 */
    REMOTE_LOG_OPERATION,   /* 領球流程 */
    REMOTE_LOG_EXCEPTION,   /* 錯誤 */
    REMOTE_LOG_INFO,
} remote_log_tag_t;

typedef struct {
    const char *dt;       /* 已格式化的時間，例如 2026-11-23T01:20:00+0800 */
    const char *level;    /* INFO／WARNING／ERROR */
    remote_log_tag_t tag;
    const char *device;
    const char *message;  /* 不含裝置與標籤前綴 */
} remote_log_entry_t;

/*
 * 把一筆寫成 JSON 物件。字串裡的 " \ 與控制字元會跳脫；UTF-8 中文原樣保留。
 * 回傳寫入的字元數（不含 \0）；空間不足或參數為 NULL 時回傳 -1，out 不保證有效。
 */
int remote_log_format_entry(char *out, size_t out_size, const remote_log_entry_t *entry);

#ifdef __cplusplus
}
#endif
