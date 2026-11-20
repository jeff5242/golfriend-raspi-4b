#pragma once
/*
 * 畫面（LVGL）。對應 Pi 版 MediaPlayer.py 的待機畫面與 4 個對話框。
 *
 * 目前是骨架：只印 log。板子型號確認後，在 ui.c 接上廠商 BSP
 * （MIPI-DSI 螢幕＋GT911 觸控）與 esp_lvgl_port，再補畫面。
 * 所有函式都可以從任何工作呼叫；實作時必須用 lvgl_port_lock() 保護。
 */
#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ui_init(void);

void ui_show_standby(void);

/* 手動輸入中：顯示目前輸入的字串 */
void ui_show_input(const char *typed);

/* 查詢中（等 take_ball 回應） */
void ui_show_querying(void);

/* 顯示取球數與剩餘點數；point 為 999999 時顯示 "------" */
void ui_show_confirm(int take, long point_before, long point_after);

/* 出球進度：第 done 盤 / 共 total 盤 */
void ui_show_dispensing(unsigned done, unsigned total);

/* 錯誤訊息（中文） */
void ui_show_error(const char *message);

/* 右上角狀態列：WiFi 連線與訊號 */
void ui_set_wifi_status(bool connected, int rssi_dbm);

#ifdef __cplusplus
}
#endif
