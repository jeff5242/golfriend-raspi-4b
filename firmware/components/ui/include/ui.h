#pragma once
/*
 * 畫面（LVGL）。對應 Pi 版 MediaPlayer.py 的待機畫面與 4 個對話框。
 *
 * 所有函式都可以從任何工作呼叫；實作時必須用 lvgl_port_lock() 保護。
 */
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ui_init(void);

/* 點了待機畫面 */
typedef void (*ui_touch_cb_t)(void *ctx);
/* 觸控鍵盤按鍵：'0'–'9'、'\b'（刪除）、'\n'（確認） */
typedef void (*ui_keypad_cb_t)(char c, void *ctx);

/* 設定觸控回呼。回呼在 LVGL 工作中執行：只能丟佇列，不得阻塞。 */
void ui_set_input_handlers(ui_touch_cb_t on_touch, ui_keypad_cb_t on_key, void *ctx);

void ui_show_standby(void);

/*
 * 手動輸入中：顯示目前輸入的字串與觸控數字鍵盤。
 * timeout_ms：沒有操作多久後回待機；畫面用倒數條顯示剩餘時間，每次呼叫重新倒數。
 */
void ui_show_input(const char *typed, uint32_t timeout_ms);

/* 查詢中（等 take_ball 回應） */
void ui_show_querying(void);

/* 顯示取球數與剩餘點數；point 為 999999 時顯示 "------" */
void ui_show_confirm(int take, long point_before, long point_after);

/* 出球進度：第 done 盤 / 共 total 盤 */
void ui_show_dispensing(unsigned done, unsigned total);

/* 系統選單（維修用）：顯示 info（多行文字）與「返回」「檢查更新」「重新開機」三顆鍵 */
typedef enum {
    UI_MENU_CLOSE = 0,
    UI_MENU_REBOOT,
    UI_MENU_UPDATE,  /* 檢查韌體更新 */
} ui_menu_action_t;
typedef void (*ui_menu_cb_t)(ui_menu_action_t action, void *ctx);

/* 回呼在 LVGL 工作中執行：只能丟佇列，不得阻塞。 */
void ui_set_menu_handler(ui_menu_cb_t on_action, void *ctx);
void ui_show_menu(const char *info);

/* 一般通知：標題＋訊息（例如「設定已更新」） */
void ui_show_notice(const char *title, const char *message);

/* 錯誤訊息（中文） */
void ui_show_error(const char *message);

/* 右上角狀態列：WiFi 名稱與訊號強度（只顯示數字，例如 "MyWiFi -56"） */
void ui_set_wifi_status(bool connected, const char *ssid, int rssi_dbm);

/* 有線網路連上時，狀態列顯示 "LAN"，不顯示 WiFi 訊號 */
void ui_set_lan_status(bool connected);

/* 待機畫面右下角的鏡頭預覽：掃描狀態（文字與框線顏色） */
typedef enum {
    UI_SCAN_IDLE = 0,  /* 請將 QR Code 對準鏡頭 */
    UI_SCAN_FOUND,     /* 偵測到 QR Code，請拿穩 */
    UI_SCAN_READ,      /* 已讀取 */
} ui_scan_status_t;

void ui_set_scan_status(ui_scan_status_t status);

/* 更新鏡頭預覽。gray 是 8 位元灰階，至少 322x182；只在待機畫面顯示時才會處理。可以從任何工作呼叫。 */
void ui_set_scan_preview(const uint8_t *gray, int w, int h);

/*
 * 除錯主控台收到畫面以外的指令字元時呼叫 handler（例如鏡頭的除錯指令）。
 * 除錯主控台關閉時（GOLF_UI_DEBUG_CONSOLE=n）不會呼叫。
 */
void ui_set_debug_key_handler(void (*handler)(int c));

#ifdef __cplusplus
}
#endif
