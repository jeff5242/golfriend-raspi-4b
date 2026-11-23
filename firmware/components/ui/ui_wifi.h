#pragma once
/*
 * WiFi 設定：待機畫面右上角的「WiFi 設定」鍵，以及按下後的設定畫面
 * （附近 WiFi 清單、密碼鍵盤、連線中）。
 * 所有函式必須在持有 lvgl_port_lock 時呼叫。
 */
#include <stdbool.h>
#include <stddef.h>

#include "lvgl.h"
#include "ui.h"

/* 建立設定畫面（預設隱藏）。在待機圖與輸入畫面之後建立，蓋在它們上面。 */
void ui_wifi_build(lv_obj_t *parent);

/* 建立右上角的「WiFi 設定」鍵（預設隱藏）。最後建立，永遠在最上層。 */
void ui_wifi_build_button(lv_obj_t *parent);

void ui_wifi_set_handler(ui_wifi_cb_t on_action, void *ctx);

void ui_wifi_set_visible(bool visible);
void ui_wifi_set_button_visible(bool visible);

void ui_wifi_show_scanning(void);
void ui_wifi_show_list(const ui_wifi_ap_t *aps, size_t count, const char *message);
void ui_wifi_show_connecting(const char *ssid);
void ui_wifi_get_selection_locked(char *ssid, size_t ssid_size, char *password, size_t password_size);
