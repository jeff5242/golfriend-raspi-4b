#pragma once
/*
 * 把逐字送進來的字元組成一整行（交易碼或卡號）。
 * 純邏輯，不依賴 ESP-IDF，可以在主機端測試。
 *
 * 規則（沿用 Pi 版 MediaPlayer.py）：
 *   - 遇到 '\n'、'\r'、'\t'、'%' 結束一行。
 *   - 空行不送出。
 *   - 超過長度上限的整行丟掉，不送出截斷的碼。
 *   - 送出一行後 1 秒內的輸入全部忽略：
 *     掃碼器會殘留 Enter，沒有這段保護會誤觸確認。
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCANNER_LINE_MAX 64
#define SCANNER_GUARD_US (1000 * 1000)

typedef struct {
    char buf[SCANNER_LINE_MAX];
    size_t len;
    bool overflow;
    int64_t guard_until_us;
} scanner_line_t;

void scanner_line_init(scanner_line_t *line);

/* 丟掉還沒結束的字元（回待機時呼叫）。1 秒按鍵保護照舊有效。 */
void scanner_line_clear(scanner_line_t *line);

/*
 * 送入一個字元。now_us 是單調遞增的時間（裝置上用 esp_timer_get_time()）。
 * 一行完成時，把內容（含結尾 '\0'）寫到 out 並回傳 true。
 * out_size 必須至少 SCANNER_LINE_MAX。
 */
bool scanner_line_feed(scanner_line_t *line, char c, int64_t now_us,
                       char *out, size_t out_size);

#ifdef __cplusplus
}
#endif
