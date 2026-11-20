#pragma once
/*
 * HID 開機協定（boot protocol）鍵盤報告 → ASCII。
 * 純邏輯，不依賴 ESP-IDF，可以在主機端測試（test_host/）。
 *
 * 掃碼器、USB 鍵盤、「鍵盤模擬」的 ID 讀卡機都送這種報告：
 *   byte0 = 修飾鍵（Shift、Ctrl…），byte1 = 保留，byte2..7 = 目前按住的鍵（最多 6 個）
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HID_KBD_KEY_SLOTS 6

/* 修飾鍵位元：左 Shift = bit1，右 Shift = bit5 */
bool hid_keymap_has_shift(uint8_t modifier);

/*
 * 把 HID usage ID 轉成 ASCII（美式鍵盤配置）。
 * Enter／數字鍵盤 Enter 回傳 '\n'，Tab 回傳 '\t'。
 * 數字鍵盤的 0–9 也轉成 '0'–'9'（部分讀卡機用數字鍵盤送卡號）。
 * 不支援的鍵回傳 0。
 */
char hid_keymap_to_ascii(uint8_t usage, bool shift);

/*
 * 比對前後兩份報告，找出這次「新按下」的鍵。
 * 報告列的是目前按住的鍵，按住不放的鍵會在下一份報告重複出現，必須去掉。
 * usage < 0x04（無按鍵、ErrorRollOver）一律忽略。
 * 回傳新按下的鍵數，鍵值依序寫到 out。
 */
size_t hid_keymap_new_keys(const uint8_t prev[HID_KBD_KEY_SLOTS],
                           const uint8_t cur[HID_KBD_KEY_SLOTS],
                           uint8_t out[HID_KBD_KEY_SLOTS]);

#ifdef __cplusplus
}
#endif
