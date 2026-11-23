#include "ui.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "ui_board.h"
#include "ui_debug.h"
#include "ui_scan.h"
#include "ui_image.h"
#include "ui_input.h"
#include "ui_theme.h"
#include "ui_wifi.h"

/*
 * 畫面：白底。一般畫面是標題＋訊息（出球時加進度條）；待機時整頁顯示待機圖；
 * 輸入畫面在 ui_input.c。右上角是網路狀態。
 * 改了畫面上的中文字串後必須重跑 tools/gen_ui_font.py。
 * TODO：
 *   1. 待機圖目前編進韌體（assets/standby.jpg）；各場館的圖之後改放 TF 卡或分割區。
 *   2. 狀態列格式沿用 Pi 版：ping 固定 3 碼、IP 用十六進位、不加 "IP:"。
 */
static const char *TAG = "ui";

#define UI_BACKLIGHT_PERCENT 80
#define UI_LOCK_TIMEOUT_MS   1000
#define UI_POINT_HIDDEN      999999L  /* 同 api.h 的 API_POINT_HIDDEN：後台不公開點數 */
#define UI_TITLE_Y           (-110)
#define UI_MESSAGE_Y         20
#define UI_PROGRESS_Y        130
#define UI_PROGRESS_WIDTH    640
#define UI_PROGRESS_HEIGHT   28
#define UI_CLOCK_PERIOD_MS   1000
#define UI_CLOCK_VALID_YEAR  2024  /* 年份小於這個值：還沒對時 */
#define UI_MENU_MESSAGE_Y    10   /* 選單的資訊有 4 行，位置比一般訊息低，避開標題 */
#define UI_MENU_TITLE_Y      (-200)
#define UI_MENU_KEY_WIDTH    240
#define UI_MENU_KEY_HEIGHT   90
#define UI_MENU_KEY_GAP      30
#define UI_MENU_KEY_BOTTOM   (-36)
#define UI_SSID_MAX          33  /* SSID 最長 32 bytes */

typedef enum {
    UI_VIEW_TEXT = 0,  /* 標題＋訊息 */
    UI_VIEW_ERROR,     /* 同上，標題用紅色 */
    UI_VIEW_INPUT,     /* 輸入的字＋鍵盤 */
    UI_VIEW_STANDBY,   /* 待機圖（沒有圖時是標題＋訊息） */
    UI_VIEW_MENU,      /* 系統選單：標題＋資訊＋兩顆鍵 */
    UI_VIEW_WIFI,      /* WiFi 設定（ui_wifi.c） */
} ui_view_t;

static bool s_ready;
static lv_obj_t *s_title;
static lv_obj_t *s_message;
static lv_obj_t *s_clock;     /* 左上角系統時間，永遠在最上層 */
static lv_obj_t *s_net;       /* 右上角網路狀態，永遠在最上層 */
static lv_obj_t *s_progress;  /* 出球進度條，只在出球畫面顯示 */
static lv_obj_t *s_standby_image;  /* 沒有待機圖時是 NULL，待機畫面退回文字 */

static lv_obj_t *s_menu_keys;      /* 系統選單的三顆鍵 */
static ui_menu_cb_t s_on_menu;
static void *s_menu_ctx;

static ui_touch_cb_t s_on_touch;
static void *s_touch_ctx;

static ui_view_t s_view = UI_VIEW_TEXT;
static bool s_lan_up;
static bool s_wifi_up;
static int s_wifi_rssi;
static char s_wifi_ssid[UI_SSID_MAX];

/*
 * 右上角：沒有網路而且在待機畫面時顯示「WiFi 設定」鍵（蓋住 "WiFi --"），其他時候顯示網路狀態。
 * 只在待機畫面顯示：領球流程中按到會打斷交易。呼叫前必須已經拿到 lvgl_port_lock。
 */
static void refresh_net_label(void)
{
    const bool offline = !s_lan_up && !s_wifi_up;
    const bool show_button = offline && s_view == UI_VIEW_STANDBY;
    ui_wifi_set_button_visible(show_button);
    lv_obj_set_hidden(s_net, show_button);
    if (s_lan_up) {
        lv_label_set_text(s_net, "LAN");
    } else if (s_wifi_up) {
        lv_label_set_text_fmt(s_net, "%s %d", s_wifi_ssid, s_wifi_rssi);
    } else {
        lv_label_set_text(s_net, "WiFi --");
    }
}

/* 在 LVGL 工作中執行 */
static void on_screen_clicked(lv_event_t *e)
{
    (void)e;
    if (s_on_touch != NULL) {
        s_on_touch(s_touch_ctx);
    }
}

static void on_menu_key(lv_event_t *e)
{
    if (s_on_menu != NULL) {
        s_on_menu((ui_menu_action_t)(uintptr_t)lv_event_get_user_data(e), s_menu_ctx);
    }
}

static void add_menu_key(const char *text, ui_menu_action_t action, uint32_t bg, uint32_t bg_down,
                         uint32_t fg)
{
    lv_obj_t *btn = lv_button_create(s_menu_keys);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, UI_MENU_KEY_WIDTH, UI_MENU_KEY_HEIGHT);
    lv_obj_set_style_radius(btn, UI_RADIUS_KEY, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg_down), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, UI_KEY_BORDER_WIDTH, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(fg), 0);
    lv_obj_add_event_cb(btn, on_menu_key, LV_EVENT_CLICKED, (void *)(uintptr_t)action);

    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_style_text_font(label, &ui_font_tc_32, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(fg), 0);
    lv_label_set_text(label, text);
    lv_obj_center(label);
}

static void build_menu_keys(lv_obj_t *scr)
{
    s_menu_keys = lv_obj_create(scr);
    lv_obj_remove_style_all(s_menu_keys);
    lv_obj_set_size(s_menu_keys, 3 * UI_MENU_KEY_WIDTH + 2 * UI_MENU_KEY_GAP, UI_MENU_KEY_HEIGHT);
    lv_obj_align(s_menu_keys, LV_ALIGN_BOTTOM_MID, 0, UI_MENU_KEY_BOTTOM);
    lv_obj_set_flex_flow(s_menu_keys, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(s_menu_keys, UI_MENU_KEY_GAP, 0);
    add_menu_key("返回", UI_MENU_CLOSE, UI_COLOR_KEY, UI_COLOR_KEY_DOWN, UI_COLOR_TEXT);
    add_menu_key("檢查更新", UI_MENU_UPDATE, UI_COLOR_KEY, UI_COLOR_KEY_DOWN, UI_COLOR_ACCENT);
    add_menu_key("重新開機", UI_MENU_REBOOT, UI_COLOR_DELETE, UI_COLOR_DELETE_DOWN,
                 UI_COLOR_DELETE_TEXT);
    lv_obj_set_hidden(s_menu_keys, true);
}

/* 每秒更新左上角時間（LVGL 計時器，在 LVGL 工作中執行）。時區由 app_main 設定。 */
static void update_clock(lv_timer_t *timer)
{
    (void)timer;
    const time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);
    if (local.tm_year + 1900 < UI_CLOCK_VALID_YEAR) {
        lv_label_set_text(s_clock, "----/--/-- --:--");  /* 還沒對時 */
        return;
    }
    lv_label_set_text_fmt(s_clock, "%04d/%02d/%02d %02d:%02d", local.tm_year + 1900,
                          local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min);
}

static void build_screen(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(scr, on_screen_clicked, LV_EVENT_CLICKED, NULL);  /* 沒有待機圖時點背景 */

    s_title = lv_label_create(scr);
    lv_obj_set_style_text_font(s_title, &ui_font_tc_56, 0);
    lv_obj_align(s_title, LV_ALIGN_CENTER, 0, UI_TITLE_Y);

    s_message = lv_label_create(scr);
    lv_obj_set_style_text_font(s_message, &ui_font_tc_32, 0);
    lv_obj_set_style_text_color(s_message, lv_color_hex(UI_COLOR_TEXT_MUTED), 0);
    lv_obj_set_style_text_align(s_message, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(s_message, 16, 0);
    lv_obj_align(s_message, LV_ALIGN_CENTER, 0, UI_MESSAGE_Y);

    s_progress = lv_bar_create(scr);
    lv_obj_set_size(s_progress, UI_PROGRESS_WIDTH, UI_PROGRESS_HEIGHT);
    lv_obj_align(s_progress, LV_ALIGN_CENTER, 0, UI_PROGRESS_Y);
    lv_obj_set_style_bg_color(s_progress, lv_color_hex(UI_COLOR_LINE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_progress, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_progress, lv_color_hex(UI_COLOR_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_hidden(s_progress, true);

    const lv_image_dsc_t *standby = NULL;
    if (ui_image_load_standby(&standby) == ESP_OK) {
        s_standby_image = lv_image_create(scr);
        lv_image_set_src(s_standby_image, standby);
        lv_obj_align(s_standby_image, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_hidden(s_standby_image, true);
        lv_obj_set_clickable(s_standby_image, true);
        lv_obj_add_event_cb(s_standby_image, on_screen_clicked, LV_EVENT_CLICKED, NULL);
    } else {
        ESP_LOGW(TAG, "standby image not available, using text");
    }

    build_menu_keys(scr);
    ui_input_build(scr, NULL, NULL);
    ui_scan_build(scr);  /* 在待機圖之後建立：蓋在待機圖上面 */
    ui_wifi_build(scr);  /* 整頁蓋住待機圖與輸入畫面 */

    /* 最後建立：蓋在待機圖與輸入畫面上面 */
    s_net = lv_label_create(scr);
    lv_obj_set_style_text_font(s_net, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_net, lv_color_hex(UI_COLOR_TEXT_MUTED), 0);
    lv_obj_align(s_net, LV_ALIGN_TOP_RIGHT, -16, 12);
    lv_label_set_text(s_net, "WiFi --");

    s_clock = lv_label_create(scr);
    lv_obj_set_style_text_font(s_clock, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_clock, lv_color_hex(UI_COLOR_TEXT_MUTED), 0);
    lv_obj_align(s_clock, LV_ALIGN_TOP_LEFT, 16, 12);
    update_clock(NULL);
    lv_timer_create(update_clock, UI_CLOCK_PERIOD_MS, NULL);

    ui_wifi_build_button(scr);  /* 最上層：蓋在網路狀態上面 */
}

/*
 * 切換畫面。可以從任何工作呼叫；螢幕沒初始化成功時回傳 false。
 * 回傳 true 時仍持有 lvgl_port_lock：呼叫端做完自己的調整後必須呼叫 lvgl_port_unlock()。
 * 整個畫面的改動必須在同一次上鎖內完成，否則 LVGL 會先畫出改到一半的版面（畫面閃動）。
 */
static bool show_begin(ui_view_t view, const char *title, const char *message)
{
    if (!s_ready || !lvgl_port_lock(UI_LOCK_TIMEOUT_MS)) {
        return false;
    }
    lv_label_set_text(s_title, title);
    lv_label_set_text(s_message, message);
    lv_obj_set_style_text_color(
        s_title, lv_color_hex(view == UI_VIEW_ERROR ? UI_COLOR_ERROR : UI_COLOR_TEXT), 0);
    lv_obj_set_hidden(s_progress, true);
    lv_obj_set_hidden(s_menu_keys, view != UI_VIEW_MENU);
    lv_obj_align(s_title, LV_ALIGN_CENTER, 0, view == UI_VIEW_MENU ? UI_MENU_TITLE_Y : UI_TITLE_Y);
    lv_obj_align(s_message, LV_ALIGN_CENTER, 0,
                 view == UI_VIEW_MENU ? UI_MENU_MESSAGE_Y : UI_MESSAGE_Y);
    ui_input_set_visible(view == UI_VIEW_INPUT);
    ui_scan_set_visible(view == UI_VIEW_STANDBY);
    ui_wifi_set_visible(view == UI_VIEW_WIFI);
    if (s_standby_image != NULL) {
        lv_obj_set_hidden(s_standby_image, view != UI_VIEW_STANDBY);
    }
    s_view = view;
    refresh_net_label();
    return true;
}

static void show(ui_view_t view, const char *title, const char *message)
{
    if (show_begin(view, title, message)) {
        lvgl_port_unlock();
    }
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
        .buffer_size = UI_LCD_H_RES * UI_LCD_V_RES,
        .double_buffer = true,
        .hres = UI_LCD_H_RES,
        .vres = UI_LCD_V_RES,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {.buff_dma = true, .buff_spiram = true, .direct_mode = true},
    };
    /*
     * 直接畫在面板的兩張畫面記憶體，畫完才在垂直同步時切換。
     * 用 direct_mode（只重畫有變的區域），不用 full_refresh（每次重畫整個螢幕）：
     * full_refresh 時一次重畫要 112 ms，輸入畫面的倒數條讓它不停重畫，
     * 按數字鍵約 1 秒才看到字（2026-11-23 實測）。LVGL 會把畫過的區域同步到另一張。
     */
    const lvgl_port_display_dsi_cfg_t dsi_cfg = {.flags = {.avoid_tearing = true}};
    lv_display_t *disp = lvgl_port_add_disp_dsi(&disp_cfg, &dsi_cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "lvgl_port_add_disp_dsi failed, running headless");
        return ESP_OK;
    }
    ui_debug_start(board.panel);
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

void ui_set_input_handlers(ui_touch_cb_t on_touch, ui_keypad_cb_t on_key, void *ctx)
{
    s_on_touch = on_touch;
    s_touch_ctx = ctx;
    if (s_ready && lvgl_port_lock(UI_LOCK_TIMEOUT_MS)) {
        ui_input_set_handler(on_key, ctx);
        lvgl_port_unlock();
    }
}

void ui_show_standby(void)
{
    ESP_LOGI(TAG, "[standby]");
    show(UI_VIEW_STANDBY, "Golfriend", "請出示 QRCode 領球");  /* 沒有待機圖時顯示這兩行 */
}

void ui_show_input(const char *typed, uint32_t timeout_ms)
{
    ESP_LOGI(TAG, "[input] %s", typed != NULL ? typed : "");
    if (show_begin(UI_VIEW_INPUT, "", "")) {
        ui_input_set_mode(false);
        ui_input_set_text(typed != NULL ? typed : "");
        ui_input_restart_countdown(timeout_ms);
        lvgl_port_unlock();
    }
}

void ui_show_scanned(const char *code, uint32_t timeout_ms)
{
    ESP_LOGI(TAG, "[scanned] %s", code != NULL ? code : "");
    if (show_begin(UI_VIEW_INPUT, "", "")) {
        ui_input_set_mode(true);
        ui_input_set_text(code != NULL ? code : "");
        ui_input_restart_countdown(timeout_ms);
        lvgl_port_unlock();
    }
}

void ui_show_querying(void)
{
    ESP_LOGI(TAG, "[querying]");
    show(UI_VIEW_TEXT, "查詢中", "請稍候");
}

void ui_show_confirm(int take, long point_before, long point_after)
{
    char text[96];
    ESP_LOGI(TAG, "[confirm] take=%d point %ld -> %ld", take, point_before, point_after);
    if (point_before == UI_POINT_HIDDEN || point_after == UI_POINT_HIDDEN) {
        snprintf(text, sizeof text, "本次領球 %d 顆\n點數 ------", take);
    } else {
        snprintf(text, sizeof text, "本次領球 %d 顆\n點數 %ld → %ld", take, point_before,
                 point_after);
    }
    show(UI_VIEW_TEXT, "領球確認", text);
}

void ui_show_dispensing(unsigned done, unsigned total)
{
    char text[64];
    ESP_LOGI(TAG, "[dispensing] %u/%u", done, total);
    if (total == 0) {
        text[0] = '\0';  /* 計時出球沒有盤數 */
    } else if (done < total) {
        snprintf(text, sizeof text, "第 %u 盤 / 共 %u 盤", done + 1, total);
    } else {
        snprintf(text, sizeof text, "全部完成，共 %u 盤", total);
    }
    if (!show_begin(UI_VIEW_TEXT, "出球中", text)) {
        return;
    }
    if (total > 0) {
        lv_bar_set_range(s_progress, 0, (int32_t)total);
        lv_bar_set_value(s_progress, (int32_t)(done < total ? done : total), LV_ANIM_OFF);
        lv_obj_set_hidden(s_progress, false);
    }
    lvgl_port_unlock();
}

void ui_set_menu_handler(ui_menu_cb_t on_action, void *ctx)
{
    s_on_menu = on_action;
    s_menu_ctx = ctx;
}

void ui_show_menu(const char *info)
{
    ESP_LOGI(TAG, "[menu]");
    show(UI_VIEW_MENU, "系統選單", info != NULL ? info : "");
}

void ui_show_notice(const char *title, const char *message)
{
    ESP_LOGI(TAG, "[notice] %s", title != NULL ? title : "");
    show(UI_VIEW_TEXT, title != NULL ? title : "", message != NULL ? message : "");
}

void ui_show_error(const char *message)
{
    ESP_LOGW(TAG, "[error] %s", message != NULL ? message : "");
    show(UI_VIEW_ERROR, "無法領球", message != NULL ? message : "");
}

void ui_set_wifi_status(bool connected, const char *ssid, int rssi_dbm)
{
    ESP_LOGI(TAG, "[wifi] %s %d dBm", connected ? "up" : "down", rssi_dbm);
    if (!s_ready || !lvgl_port_lock(UI_LOCK_TIMEOUT_MS)) {
        return;
    }
    s_wifi_up = connected;
    s_wifi_rssi = rssi_dbm;
    snprintf(s_wifi_ssid, sizeof s_wifi_ssid, "%s", ssid != NULL ? ssid : "");
    refresh_net_label();
    lvgl_port_unlock();
}

void ui_set_lan_status(bool connected)
{
    ESP_LOGI(TAG, "[lan] %s", connected ? "up" : "down");
    if (!s_ready || !lvgl_port_lock(UI_LOCK_TIMEOUT_MS)) {
        return;
    }
    s_lan_up = connected;
    refresh_net_label();
    lvgl_port_unlock();
}

void ui_set_wifi_handler(ui_wifi_cb_t on_action, void *ctx)
{
    if (s_ready && lvgl_port_lock(UI_LOCK_TIMEOUT_MS)) {
        ui_wifi_set_handler(on_action, ctx);
        lvgl_port_unlock();
    }
}

void ui_show_wifi_scanning(void)
{
    ESP_LOGI(TAG, "[wifi settings] scanning");
    if (show_begin(UI_VIEW_WIFI, "", "")) {
        ui_wifi_show_scanning();
        lvgl_port_unlock();
    }
}

void ui_show_wifi_list(const ui_wifi_ap_t *aps, size_t count, const char *message)
{
    ESP_LOGI(TAG, "[wifi settings] %u networks%s%s", (unsigned)count, message != NULL ? ": " : "",
             message != NULL ? message : "");
    if (show_begin(UI_VIEW_WIFI, "", "")) {
        ui_wifi_show_list(aps, count, message);
        lvgl_port_unlock();
    }
}

void ui_show_wifi_connecting(const char *ssid)
{
    ESP_LOGI(TAG, "[wifi settings] connecting to %s", ssid != NULL ? ssid : "");
    if (show_begin(UI_VIEW_WIFI, "", "")) {
        ui_wifi_show_connecting(ssid);
        lvgl_port_unlock();
    }
}

bool ui_wifi_get_selection(char *ssid, size_t ssid_size, char *password, size_t password_size)
{
    if (!s_ready || !lvgl_port_lock(UI_LOCK_TIMEOUT_MS)) {
        return false;
    }
    ui_wifi_get_selection_locked(ssid, ssid_size, password, password_size);
    lvgl_port_unlock();
    return true;
}
