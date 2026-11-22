#pragma once
/*
 * 輸入畫面：左欄顯示標題與輸入的領球碼，右邊是數字鍵，最下排是「刪除／0／確認」。
 * 所有函式必須在持有 lvgl_port_lock 時呼叫。
 */
#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"
#include "ui.h"

/* 建立輸入畫面（預設隱藏）。on_key 在 LVGL 工作中執行。 */
void ui_input_build(lv_obj_t *parent, ui_keypad_cb_t on_key, void *ctx);

void ui_input_set_handler(ui_keypad_cb_t on_key, void *ctx);
void ui_input_set_text(const char *typed);
/* 隱藏時停止倒數動畫 */
void ui_input_set_visible(bool visible);

/* 倒數條從滿格開始，timeout_ms 後歸零 */
void ui_input_restart_countdown(uint32_t timeout_ms);
