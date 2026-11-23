#pragma once
/*
 * 畫面（LVGL）。對應 Pi 版 MediaPlayer.py 的待機畫面與 4 個對話框。
 *
 * 所有函式都可以從任何工作呼叫；實作時必須用 lvgl_port_lock() 保護。
 */
#include <stdbool.h>
#include <stddef.h>
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

/*
 * WiFi 設定。沒有網路（有線與 WiFi 都沒連上）時，待機畫面右上角出現「WiFi 設定」鍵。
 * 按下後是附近 WiFi 的清單；點需要密碼的網路會出現鍵盤輸入密碼，點開放網路直接連線。
 */
typedef enum {
    UI_WIFI_OPEN = 0,  /* 按了待機畫面的 WiFi 設定鍵 */
    UI_WIFI_CLOSE,     /* 按了「返回」離開設定 */
    UI_WIFI_RESCAN,    /* 按了「重新掃描」 */
    UI_WIFI_CONNECT,   /* 選好網路（與密碼）：用 ui_wifi_get_selection() 取 */
    UI_WIFI_ACTIVITY,  /* 打字、點選等操作，主流程用來重新計時 */
} ui_wifi_action_t;

#define UI_WIFI_SSID_MAX     33  /* SSID 最長 32 bytes */
#define UI_WIFI_PASSWORD_MAX 65  /* WPA2 密碼最長 64 bytes */
#define UI_WIFI_LIST_MAX     20  /* 清單最多列幾個 */

typedef struct {
    char ssid[UI_WIFI_SSID_MAX];
    int rssi;      /* dBm */
    bool secured;  /* 需要密碼 */
} ui_wifi_ap_t;

/* 回呼在 LVGL 工作中執行：只能丟佇列，不得阻塞。 */
typedef void (*ui_wifi_cb_t)(ui_wifi_action_t action, void *ctx);
void ui_set_wifi_handler(ui_wifi_cb_t on_action, void *ctx);

/* 掃描中（清單清空、顯示「掃描中」） */
void ui_show_wifi_scanning(void);

/* 顯示掃描結果。message 不是 NULL 時顯示在清單上方（例如「密碼錯誤」）。aps 會被複製。 */
void ui_show_wifi_list(const ui_wifi_ap_t *aps, size_t count, const char *message);

/* 連線中 */
void ui_show_wifi_connecting(const char *ssid);

/* 取得使用者選的網路與輸入的密碼（收到 UI_WIFI_CONNECT 後呼叫）。畫面沒初始化時回傳 false。 */
bool ui_wifi_get_selection(char *ssid, size_t ssid_size, char *password, size_t password_size);

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
