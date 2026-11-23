#include "ui_wifi.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ui_board.h"
#include "ui_theme.h"

/*
 * 版面（1024x600）：最上面 40 是狀態列（時間、網路），下面是標題列（標題＋右邊的按鍵），
 * 再下面依頁面顯示：WiFi 清單／密碼輸入（輸入框＋鍵盤）／連線中。
 * SSID 裡的中文不在字型裡（字型只收錄韌體字串用到的字），會顯示不出來；英數字正常。
 */
#define PAD_X            40
#define HEADER_Y         44
#define HEADER_BTN_W     180
#define HEADER_BTN_H     64
#define HEADER_BTN_GAP   20
#define TITLE_Y          56
#define STATUS_Y         124
#define WIFI_LIST_Y           164
#define WIFI_LIST_W           (UI_LCD_H_RES - 2 * PAD_X)
#define WIFI_LIST_H           (UI_LCD_V_RES - WIFI_LIST_Y - 16)
#define ROW_H            80
#define ROW_GAP          10
#define ROW_PAD_X        24
#define ROW_SSID_W       560
#define PAGE_Y           124
#define PAGE_H           (UI_LCD_V_RES - PAGE_Y)
#define PW_TEXTAREA_Y    40
#define PW_TEXTAREA_H    72
#define KEYBOARD_H       330
#define SPINNER_SIZE     80
#define OPEN_BTN_W       200
#define OPEN_BTN_H       56
#define RSSI_STRONG      (-60)
#define RSSI_MEDIUM      (-75)

typedef enum {
    PAGE_LIST = 0,
    PAGE_PASSWORD,
    PAGE_CONNECTING,
} page_t;

static lv_obj_t *s_root;
static lv_obj_t *s_title;
static lv_obj_t *s_btn_rescan;
static lv_obj_t *s_btn_connect;
static lv_obj_t *s_btn_back;
static lv_obj_t *s_status;
static lv_obj_t *s_list;
static lv_obj_t *s_pw_page;
static lv_obj_t *s_pw_ssid;
static lv_obj_t *s_pw_text;
static lv_obj_t *s_connecting_page;
static lv_obj_t *s_connecting_text;
static lv_obj_t *s_open_btn;
static page_t s_page;

static ui_wifi_cb_t s_on_action;
static void *s_ctx;

static ui_wifi_ap_t s_aps[UI_WIFI_LIST_MAX];
static size_t s_ap_count;
static char s_sel_ssid[UI_WIFI_SSID_MAX];
static char s_sel_password[UI_WIFI_PASSWORD_MAX];

static void fire(ui_wifi_action_t action)
{
    if (s_on_action != NULL) {
        s_on_action(action, s_ctx);
    }
}

static void plain(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_scrollable(obj, false);
}

static lv_obj_t *add_button(lv_obj_t *parent, const char *text, uint32_t bg, uint32_t bg_down,
                            uint32_t fg, uint32_t border, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_style_radius(btn, UI_RADIUS_KEY, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg_down), LV_STATE_PRESSED);
    lv_obj_set_style_opa(btn, LV_OPA_50, LV_STATE_DISABLED);
    lv_obj_set_style_border_width(btn, UI_KEY_BORDER_WIDTH, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(border), 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_style_text_font(label, &ui_font_tc_32, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(fg), 0);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
}

/* 標題列右邊的按鍵：由右往左排，index 0 是最右邊 */
static void place_header_button(lv_obj_t *btn, int index)
{
    lv_obj_set_size(btn, HEADER_BTN_W, HEADER_BTN_H);
    lv_obj_set_pos(btn, UI_LCD_H_RES - PAD_X - HEADER_BTN_W - index * (HEADER_BTN_W + HEADER_BTN_GAP),
                   HEADER_Y);
}

static void show_page(page_t page)
{
    s_page = page;
    lv_obj_set_hidden(s_status, page != PAGE_LIST);
    lv_obj_set_hidden(s_list, page != PAGE_LIST);
    lv_obj_set_hidden(s_pw_page, page != PAGE_PASSWORD);
    lv_obj_set_hidden(s_connecting_page, page != PAGE_CONNECTING);
    lv_obj_set_hidden(s_btn_rescan, page != PAGE_LIST);
    lv_obj_set_hidden(s_btn_connect, page != PAGE_PASSWORD);
    lv_label_set_text(s_title, page == PAGE_PASSWORD ? "輸入密碼" : "WiFi 設定");
}

/* ---- 事件（在 LVGL 工作中執行） ---------------------------------------- */

static void on_open_clicked(lv_event_t *e)
{
    (void)e;
    fire(UI_WIFI_OPEN);
}

static void on_back_clicked(lv_event_t *e)
{
    (void)e;
    if (s_page == PAGE_PASSWORD) {
        show_page(PAGE_LIST);  /* 輸入密碼時「返回」回到清單，不離開設定 */
        fire(UI_WIFI_ACTIVITY);
        return;
    }
    fire(UI_WIFI_CLOSE);  /* 連線中按「返回」等於取消 */
}

static void on_rescan_clicked(lv_event_t *e)
{
    (void)e;
    fire(UI_WIFI_RESCAN);
}

static void submit_password(void)
{
    snprintf(s_sel_password, sizeof s_sel_password, "%s", lv_textarea_get_text(s_pw_text));
    fire(UI_WIFI_CONNECT);
}

static void on_connect_clicked(lv_event_t *e)
{
    (void)e;
    submit_password();
}

static void on_keyboard_event(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        submit_password();  /* 鍵盤上的 ✓ */
    } else if (code == LV_EVENT_CANCEL) {
        show_page(PAGE_LIST);  /* 鍵盤上的收起鍵 */
        fire(UI_WIFI_ACTIVITY);
    }
}

static void on_password_changed(lv_event_t *e)
{
    (void)e;
    fire(UI_WIFI_ACTIVITY);  /* 打字中不得逾時回待機 */
}

static void on_row_clicked(lv_event_t *e)
{
    const size_t index = (size_t)(uintptr_t)lv_event_get_user_data(e);
    if (index >= s_ap_count) {
        return;
    }
    const ui_wifi_ap_t *ap = &s_aps[index];
    snprintf(s_sel_ssid, sizeof s_sel_ssid, "%s", ap->ssid);
    s_sel_password[0] = '\0';
    if (!ap->secured) {
        fire(UI_WIFI_CONNECT);  /* 開放網路：直接連 */
        return;
    }
    lv_label_set_text_fmt(s_pw_ssid, "WiFi：%s", ap->ssid);
    lv_textarea_set_text(s_pw_text, "");
    show_page(PAGE_PASSWORD);
    fire(UI_WIFI_ACTIVITY);
}

/* ---- 建立 -------------------------------------------------------------- */

static void build_header(void)
{
    s_title = lv_label_create(s_root);
    lv_obj_set_style_text_font(s_title, &ui_font_tc_32, 0);
    lv_obj_set_style_text_color(s_title, lv_color_hex(UI_COLOR_TEXT), 0);
    lv_obj_set_pos(s_title, PAD_X, TITLE_Y);

    s_btn_back = add_button(s_root, "返回", UI_COLOR_KEY, UI_COLOR_KEY_DOWN, UI_COLOR_TEXT,
                            UI_COLOR_KEY_BORDER, on_back_clicked);
    place_header_button(s_btn_back, 0);
    s_btn_rescan = add_button(s_root, "重新掃描", UI_COLOR_KEY, UI_COLOR_KEY_DOWN, UI_COLOR_ACCENT,
                              UI_COLOR_ACCENT, on_rescan_clicked);
    place_header_button(s_btn_rescan, 1);
    s_btn_connect = add_button(s_root, "連線", UI_COLOR_ACCENT, UI_COLOR_ACCENT_DOWN, UI_COLOR_BG,
                               UI_COLOR_ACCENT_DOWN, on_connect_clicked);
    place_header_button(s_btn_connect, 1);
}

static void build_list_page(void)
{
    s_status = lv_label_create(s_root);
    lv_obj_set_style_text_font(s_status, &ui_font_tc_24, 0);
    lv_obj_set_style_text_color(s_status, lv_color_hex(UI_COLOR_TEXT_MUTED), 0);
    lv_obj_set_pos(s_status, PAD_X, STATUS_Y);
    lv_label_set_text(s_status, "");

    s_list = lv_obj_create(s_root);
    lv_obj_remove_style_all(s_list);
    lv_obj_set_pos(s_list, PAD_X, WIFI_LIST_Y);
    lv_obj_set_size(s_list, WIFI_LIST_W, WIFI_LIST_H);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_list, ROW_GAP, 0);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);  /* 網路多時上下滑 */
}

static void build_password_page(void)
{
    s_pw_page = lv_obj_create(s_root);
    plain(s_pw_page);
    lv_obj_set_pos(s_pw_page, 0, PAGE_Y);
    lv_obj_set_size(s_pw_page, UI_LCD_H_RES, PAGE_H);

    s_pw_ssid = lv_label_create(s_pw_page);
    lv_obj_set_style_text_font(s_pw_ssid, &ui_font_tc_24, 0);
    lv_obj_set_style_text_color(s_pw_ssid, lv_color_hex(UI_COLOR_TEXT_MUTED), 0);
    lv_obj_set_pos(s_pw_ssid, PAD_X, 0);

    s_pw_text = lv_textarea_create(s_pw_page);
    lv_obj_set_pos(s_pw_text, PAD_X, PW_TEXTAREA_Y);
    lv_obj_set_size(s_pw_text, WIFI_LIST_W, PW_TEXTAREA_H);
    lv_textarea_set_one_line(s_pw_text, true);
    lv_textarea_set_max_length(s_pw_text, UI_WIFI_PASSWORD_MAX - 1);
    lv_textarea_set_placeholder_text(s_pw_text, "請輸入密碼");
    lv_obj_set_style_text_font(s_pw_text, &ui_font_tc_32, 0);
    lv_obj_set_style_border_color(s_pw_text, lv_color_hex(UI_COLOR_ACCENT), 0);
    lv_obj_add_state(s_pw_text, LV_STATE_FOCUSED);  /* 顯示游標 */
    lv_obj_add_event_cb(s_pw_text, on_password_changed, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *kb = lv_keyboard_create(s_pw_page);
    lv_obj_set_size(kb, UI_LCD_H_RES, KEYBOARD_H);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_20, LV_PART_ITEMS);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(kb, s_pw_text);
    lv_obj_add_event_cb(kb, on_keyboard_event, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(kb, on_keyboard_event, LV_EVENT_CANCEL, NULL);
}

static void build_connecting_page(void)
{
    s_connecting_page = lv_obj_create(s_root);
    plain(s_connecting_page);
    lv_obj_set_pos(s_connecting_page, 0, PAGE_Y);
    lv_obj_set_size(s_connecting_page, UI_LCD_H_RES, PAGE_H);

    lv_obj_t *spinner = lv_spinner_create(s_connecting_page);
    lv_obj_set_size(spinner, SPINNER_SIZE, SPINNER_SIZE);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, -SPINNER_SIZE);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(UI_COLOR_ACCENT), LV_PART_INDICATOR);

    s_connecting_text = lv_label_create(s_connecting_page);
    lv_obj_set_style_text_font(s_connecting_text, &ui_font_tc_32, 0);
    lv_obj_set_style_text_color(s_connecting_text, lv_color_hex(UI_COLOR_TEXT), 0);
    lv_obj_set_style_text_align(s_connecting_text, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_connecting_text, LV_ALIGN_CENTER, 0, SPINNER_SIZE / 2);
}

void ui_wifi_build(lv_obj_t *parent)
{
    s_root = lv_obj_create(parent);
    plain(s_root);
    lv_obj_set_size(s_root, UI_LCD_H_RES, UI_LCD_V_RES);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);

    build_header();
    build_list_page();
    build_password_page();
    build_connecting_page();
    show_page(PAGE_LIST);
    lv_obj_set_hidden(s_root, true);
}

void ui_wifi_build_button(lv_obj_t *parent)
{
    s_open_btn = add_button(parent, "WiFi 設定", UI_COLOR_ACCENT, UI_COLOR_ACCENT_DOWN, UI_COLOR_BG,
                            UI_COLOR_ACCENT_DOWN, on_open_clicked);
    lv_obj_set_size(s_open_btn, OPEN_BTN_W, OPEN_BTN_H);
    lv_obj_align(s_open_btn, LV_ALIGN_TOP_RIGHT, -16, 8);
    lv_obj_set_hidden(s_open_btn, true);
}

void ui_wifi_set_handler(ui_wifi_cb_t on_action, void *ctx)
{
    s_on_action = on_action;
    s_ctx = ctx;
}

void ui_wifi_set_visible(bool visible)
{
    lv_obj_set_hidden(s_root, !visible);
}

void ui_wifi_set_button_visible(bool visible)
{
    lv_obj_set_hidden(s_open_btn, !visible);
}

/* ---- 頁面內容 ---------------------------------------------------------- */

static const char *signal_text(int rssi)
{
    return rssi >= RSSI_STRONG ? "訊號強" : rssi >= RSSI_MEDIUM ? "訊號中" : "訊號弱";
}

static void add_row(size_t index)
{
    const ui_wifi_ap_t *ap = &s_aps[index];
    lv_obj_t *row = lv_button_create(s_list);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), ROW_H);
    lv_obj_set_style_radius(row, UI_RADIUS_KEY, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(UI_COLOR_KEY), 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(UI_COLOR_KEY_DOWN), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(row, UI_KEY_BORDER_WIDTH, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(UI_COLOR_KEY_BORDER), 0);
    lv_obj_add_event_cb(row, on_row_clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)index);

    lv_obj_t *ssid = lv_label_create(row);
    lv_obj_set_style_text_font(ssid, &ui_font_tc_32, 0);
    lv_obj_set_style_text_color(ssid, lv_color_hex(UI_COLOR_TEXT), 0);
    lv_obj_set_width(ssid, ROW_SSID_W);
    lv_label_set_long_mode(ssid, LV_LABEL_LONG_MODE_DOTS);
    lv_label_set_text(ssid, ap->ssid);
    lv_obj_align(ssid, LV_ALIGN_LEFT_MID, ROW_PAD_X, 0);

    lv_obj_t *info = lv_label_create(row);
    lv_obj_set_style_text_font(info, &ui_font_tc_24, 0);
    lv_obj_set_style_text_color(info, lv_color_hex(UI_COLOR_TEXT_MUTED), 0);
    lv_label_set_text_fmt(info, "%s　%s", signal_text(ap->rssi), ap->secured ? "需要密碼" : "開放");
    lv_obj_align(info, LV_ALIGN_RIGHT_MID, -ROW_PAD_X, 0);
}

void ui_wifi_show_scanning(void)
{
    lv_obj_clean(s_list);
    s_ap_count = 0;
    lv_label_set_text(s_status, "掃描中，請稍候");
    lv_obj_add_state(s_btn_rescan, LV_STATE_DISABLED);
    show_page(PAGE_LIST);
}

void ui_wifi_show_list(const ui_wifi_ap_t *aps, size_t count, const char *message)
{
    lv_obj_clean(s_list);
    s_ap_count = aps == NULL ? 0 : count < UI_WIFI_LIST_MAX ? count : UI_WIFI_LIST_MAX;
    if (s_ap_count > 0) {
        memcpy(s_aps, aps, s_ap_count * sizeof s_aps[0]);
    }
    for (size_t i = 0; i < s_ap_count; i++) {
        add_row(i);
    }
    if (message != NULL) {
        lv_label_set_text(s_status, message);
    } else {
        lv_label_set_text(s_status, s_ap_count > 0 ? "請點選要連線的 WiFi" : "附近沒有 WiFi");
    }
    lv_obj_remove_state(s_btn_rescan, LV_STATE_DISABLED);
    lv_obj_scroll_to_y(s_list, 0, LV_ANIM_OFF);
    show_page(PAGE_LIST);
}

void ui_wifi_show_connecting(const char *ssid)
{
    lv_label_set_text_fmt(s_connecting_text, "連線中，請稍候\n%s", ssid != NULL ? ssid : "");
    show_page(PAGE_CONNECTING);
}

void ui_wifi_get_selection_locked(char *ssid, size_t ssid_size, char *password, size_t password_size)
{
    if (ssid != NULL && ssid_size > 0) {
        snprintf(ssid, ssid_size, "%s", s_sel_ssid);
    }
    if (password != NULL && password_size > 0) {
        snprintf(password, password_size, "%s", s_sel_password);
    }
}
