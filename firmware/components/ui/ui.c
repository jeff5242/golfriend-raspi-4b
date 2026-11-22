#include "ui.h"

#include <stdio.h>

#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "ui_board.h"

/*
 * 畫面第一版：一個標題、一行訊息、右上角 WiFi 狀態。先確認螢幕點得亮、流程看得到。
 * TODO（原型步驟 5）：
 *   1. 用 lv_font_conv 把畫面用到的中文字轉成字型（Noto Sans TC 子集）。
 *      在那之前畫面只顯示英數字；中文錯誤訊息只印在 log。
 *   2. 待機圖用 JPEG 放 Flash 或 TF 卡，以硬體 JPEG 解碼。
 *   3. 狀態列格式沿用 Pi 版：ping 固定 3 碼、IP 用十六進位、不加 "IP:"。
 */
static const char *TAG = "ui";

#define UI_BACKLIGHT_PERCENT 80
#define UI_LOCK_TIMEOUT_MS   1000

static bool s_ready;
static lv_obj_t *s_title;
static lv_obj_t *s_message;
static lv_obj_t *s_wifi;

static void build_screen(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0B3D2E), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    s_title = lv_label_create(scr);
    lv_obj_set_style_text_font(s_title, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_title, lv_color_white(), 0);
    lv_obj_align(s_title, LV_ALIGN_CENTER, 0, -60);

    s_message = lv_label_create(scr);
    lv_obj_set_style_text_font(s_message, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_message, lv_color_hex(0xD8F3DC), 0);
    lv_obj_align(s_message, LV_ALIGN_CENTER, 0, 30);

    s_wifi = lv_label_create(scr);
    lv_obj_set_style_text_font(s_wifi, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_wifi, lv_color_white(), 0);
    lv_obj_align(s_wifi, LV_ALIGN_TOP_RIGHT, -16, 12);
    lv_label_set_text(s_wifi, "WiFi --");
}

/* 可以從任何工作呼叫；螢幕沒初始化成功時只略過 */
static void show(const char *title, const char *message)
{
    if (!s_ready || !lvgl_port_lock(UI_LOCK_TIMEOUT_MS)) {
        return;
    }
    lv_label_set_text(s_title, title);
    lv_label_set_text(s_message, message);
    lvgl_port_unlock();
}

esp_err_t ui_init(void)
{
    ui_board_t board;
    esp_err_t err = ui_board_init(&board);
    if (err != ESP_OK) {
        /* 沒有螢幕時流程照跑，畫面只印 log */
        ESP_LOGE(TAG, "display init failed: %s, running headless", esp_err_to_name(err));
        return ESP_OK;
    }

    const lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    err = lvgl_port_init(&port_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "lvgl_port_init: %s, running headless", esp_err_to_name(err));
        return ESP_OK;
    }

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = board.io,
        .panel_handle = board.panel,
        .buffer_size = UI_LCD_H_RES * 100,  /* 一次畫 100 行 */
        .double_buffer = false,
        .hres = UI_LCD_H_RES,
        .vres = UI_LCD_V_RES,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {.buff_dma = true, .buff_spiram = true},
    };
    const lvgl_port_display_dsi_cfg_t dsi_cfg = {.flags = {.avoid_tearing = false}};
    lv_display_t *disp = lvgl_port_add_disp_dsi(&disp_cfg, &dsi_cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "lvgl_port_add_disp_dsi failed, running headless");
        return ESP_OK;
    }
    if (board.touch != NULL) {
        const lvgl_port_touch_cfg_t touch_cfg = {.disp = disp, .handle = board.touch};
        if (lvgl_port_add_touch(&touch_cfg) == NULL) {
            ESP_LOGW(TAG, "lvgl_port_add_touch failed");
        }
    }

    if (lvgl_port_lock(UI_LOCK_TIMEOUT_MS)) {
        build_screen();
        lvgl_port_unlock();
        s_ready = true;
    }
    err = ui_board_set_backlight(UI_BACKLIGHT_PERCENT);
    ESP_LOGI(TAG, "display ready, backlight %d%% (%s)", UI_BACKLIGHT_PERCENT, esp_err_to_name(err));
    return ESP_OK;
}

void ui_show_standby(void)
{
    ESP_LOGI(TAG, "[standby]");
    show("Golfriend", "Scan QR code or card");
}

void ui_show_input(const char *typed)
{
    ESP_LOGI(TAG, "[input] %s", typed != NULL ? typed : "");
    show("Code", typed != NULL ? typed : "");
}

void ui_show_querying(void)
{
    ESP_LOGI(TAG, "[querying]");
    show("Checking...", "");
}

void ui_show_confirm(int take, long point_before, long point_after)
{
    char text[64];
    ESP_LOGI(TAG, "[confirm] take=%d point %ld -> %ld", take, point_before, point_after);
    snprintf(text, sizeof text, "%d balls   %ld -> %ld", take, point_before, point_after);
    show("Confirm", text);
}

void ui_show_dispensing(unsigned done, unsigned total)
{
    char text[32];
    ESP_LOGI(TAG, "[dispensing] %u/%u", done, total);
    snprintf(text, sizeof text, "%u / %u", done, total);
    show("Dispensing", text);
}

void ui_show_error(const char *message)
{
    ESP_LOGW(TAG, "[error] %s", message != NULL ? message : "");
    show("Error", "Please contact staff");  /* 中文訊息要等字型做好，先看 log */
}

void ui_set_wifi_status(bool connected, int rssi_dbm)
{
    ESP_LOGI(TAG, "[wifi] %s %d dBm", connected ? "up" : "down", rssi_dbm);
    if (!s_ready || !lvgl_port_lock(UI_LOCK_TIMEOUT_MS)) {
        return;
    }
    if (connected) {
        lv_label_set_text_fmt(s_wifi, "WiFi %d dBm", rssi_dbm);
    } else {
        lv_label_set_text(s_wifi, "WiFi --");
    }
    lvgl_port_unlock();
}
