#pragma once
/*
 * 每個輸入裝置各自的鍵盤狀態與行緩衝。
 * 純邏輯，不依賴 ESP-IDF，可以在主機端測試。
 *
 * 經 USB Hub 同時接掃碼器、鍵盤、讀卡機時，每台裝置的「上一份報告」和沒打完的字
 * 必須分開存：共用時兩台同時送字會混成一行，或把另一台按住的鍵當成沒放開而漏字。
 * 呼叫端必須在同一個工作中使用同一份表（USB HID 回呼工作），不必上鎖。
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hid_keymap.h"
#include "scanner_line.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SCANNER_MAX_DEVICES 4  /* 掃碼器、鍵盤、讀卡機，加一個序列埠讀卡機 */

typedef struct {
    const void *key;                        /* 裝置識別（HID handle）；NULL = 空位 */
    uint8_t prev_keys[HID_KBD_KEY_SLOTS];
    scanner_line_t line;
    uint32_t reset_gen;                     /* 已處理到第幾次「回待機清殘字」 */
} scanner_dev_t;

typedef struct {
    scanner_dev_t slot[SCANNER_MAX_DEVICES];
} scanner_devs_t;

void scanner_devs_init(scanner_devs_t *devs);

/*
 * 找出 key 的狀態；第一次出現時佔一個空位，從空行開始，reset_gen 記成目前值。
 * key 為 NULL 或表已滿時回傳 NULL。
 */
scanner_dev_t *scanner_devs_get(scanner_devs_t *devs, const void *key, uint32_t reset_gen);

/* 裝置拔掉時呼叫：只清掉這台的狀態。 */
void scanner_devs_remove(scanner_devs_t *devs, const void *key);

/* 處理這台裝置的一份 HID 開機協定鍵盤報告（8 bytes），回傳新按下的字元數，字元寫到 out。 */
size_t scanner_dev_report_chars(scanner_dev_t *dev, const uint8_t *report, size_t len,
                                char out[HID_KBD_KEY_SLOTS]);

/*
 * 把一個字元送進這台裝置的行緩衝，規則同 scanner_line_feed。
 * reset_gen 和上次不同（回待機過）時，先用 scanner_line_clear_stale 清掉閒置的殘字。
 */
bool scanner_dev_feed(scanner_dev_t *dev, char c, int64_t now_us, uint32_t reset_gen,
                      char *out, size_t out_size);

#ifdef __cplusplus
}
#endif
