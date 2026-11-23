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

#ifdef __cplusplus
}
#endif
