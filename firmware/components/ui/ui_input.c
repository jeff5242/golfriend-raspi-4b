#include "ui_input.h"

#include <stdint.h>

#include "ui_board.h"
#include "ui_theme.h"

/* 版面（1024x600）：左欄 400，右邊數字鍵 3 欄，最右一排功能鍵 */
#define PANEL_WIDTH     400
#define PANEL_PAD_X     30
#define TITLE_Y         150
#define CODE_Y          250
#define UNDERLINE_Y     332
#define UNDERLINE_H     4
#define COUNTDOWN_Y     360
#define COUNTDOWN_H     12
#define HINT_Y          384
#define TEXT_WIDTH      (PANEL_WIDTH - 2 * PANEL_PAD_X)
#define PAD_X           424
#define PAD_TOP         44
#define PAD_WIDTH       576
#define PAD_HEIGHT      512
#define PAD_COLS        3
#define PAD_ROWS        4
#define KEY_GAP         12
#define KEY_WIDTH       ((PAD_WIDTH - (PAD_COLS - 1) * KEY_GAP) / PAD_COLS)
#define KEY_HEIGHT      ((PAD_HEIGHT - (PAD_ROWS - 1) * KEY_GAP) / PAD_ROWS)

static lv_obj_t *s_root;
static lv_obj_t *s_code;
static lv_obj_t *s_countdown;
static lv_obj_t *s_title;
static lv_obj_t *s_hint;
static ui_keypad_cb_t s_on_key;
static void *s_ctx;

static void send_key(char c)
{
    if (s_on_key != NULL) {
        s_on_key(c, s_ctx);
    }
}

static void on_key_clicked(lv_event_t *e)
{
    send_key((char)(uintptr_t)lv_event_get_user_data(e));
}

static void plain(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_scrollable(obj, false);
}

static lv_obj_t *add_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int y,
                           const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_pos(label, PANEL_PAD_X, y);
    lv_label_set_text(label, text);
    return label;
}

static void build_panel(void)
{
    lv_obj_t *panel = lv_obj_create(s_root);
    plain(panel);
    lv_obj_set_size(panel, PANEL_WIDTH, UI_LCD_V_RES);
    lv_obj_set_style_bg_color(panel, lv_color_hex(UI_COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(panel, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(UI_COLOR_LINE), 0);

    s_title = add_label(panel, &ui_font_tc_56, UI_COLOR_TEXT, TITLE_Y, "請輸入領球碼");
    s_code = add_label(panel, &ui_font_tc_56, UI_COLOR_TEXT, CODE_Y, "");

    lv_obj_t *underline = lv_obj_create(panel);
    plain(underline);
    lv_obj_set_pos(underline, PANEL_PAD_X, UNDERLINE_Y);
    lv_obj_set_size(underline, TEXT_WIDTH, UNDERLINE_H);
    lv_obj_set_style_bg_color(underline, lv_color_hex(UI_COLOR_ACCENT), 0);
    lv_obj_set_style_bg_opa(underline, LV_OPA_COVER, 0);

    s_countdown = lv_bar_create(panel);
    lv_obj_set_pos(s_countdown, PANEL_PAD_X, COUNTDOWN_Y);
    lv_obj_set_size(s_countdown, TEXT_WIDTH, COUNTDOWN_H);
    lv_obj_set_style_bg_color(s_countdown, lv_color_hex(UI_COLOR_LINE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_countdown, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_countdown, lv_color_hex(UI_COLOR_ACCENT), LV_PART_INDICATOR);

    s_hint = add_label(panel, &ui_font_tc_24, UI_COLOR_TEXT_MUTED, HINT_Y, "倒數結束會回到待機");
}

typedef struct {
    const char *text;
    char key;
    uint32_t bg;
    uint32_t bg_down;
    uint32_t fg;
    uint32_t border;
    const lv_font_t *font;
} key_style_t;

/* 每顆鍵是獨立的按鈕（不用 buttonmatrix）：刪除、確認要有自己的顏色 */
static void add_key(const key_style_t *k, int col, int row)
{
    lv_obj_t *btn = lv_button_create(s_root);
    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, PAD_X + col * (KEY_WIDTH + KEY_GAP), PAD_TOP + row * (KEY_HEIGHT + KEY_GAP));
    lv_obj_set_size(btn, KEY_WIDTH, KEY_HEIGHT);
    lv_obj_set_style_radius(btn, UI_RADIUS_KEY, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(k->bg), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(k->bg_down), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, UI_KEY_BORDER_WIDTH, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(k->border), 0);
    lv_obj_add_event_cb(btn, on_key_clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)k->key);

    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_style_text_font(label, k->font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(k->fg), 0);
    lv_label_set_text(label, k->text);
    lv_obj_center(label);
}

static void build_keys(void)
{
    static const char *const digits[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};
    for (int i = 0; i < 9; i++) {
        const key_style_t k = {digits[i], digits[i][0], UI_COLOR_KEY, UI_COLOR_KEY_DOWN,
                               UI_COLOR_TEXT, UI_COLOR_KEY_BORDER, &ui_font_tc_56};
        add_key(&k, i % PAD_COLS, i / PAD_COLS);
    }
    const key_style_t bottom[] = {
        {"刪除", '\b', UI_COLOR_DELETE, UI_COLOR_DELETE_DOWN, UI_COLOR_DELETE_TEXT,
         UI_COLOR_DELETE_TEXT, &ui_font_tc_32},
        {"0", '0', UI_COLOR_KEY, UI_COLOR_KEY_DOWN, UI_COLOR_TEXT, UI_COLOR_KEY_BORDER,
         &ui_font_tc_56},
        {"確認", '\n', UI_COLOR_ACCENT, UI_COLOR_ACCENT_DOWN, UI_COLOR_BG, UI_COLOR_ACCENT_DOWN,
         &ui_font_tc_32},
    };
    for (int col = 0; col < PAD_COLS; col++) {
        add_key(&bottom[col], col, PAD_ROWS - 1);
    }
}

void ui_input_build(lv_obj_t *parent, ui_keypad_cb_t on_key, void *ctx)
{
    s_on_key = on_key;
    s_ctx = ctx;

    s_root = lv_obj_create(parent);
    plain(s_root);
    lv_obj_set_size(s_root, UI_LCD_H_RES, UI_LCD_V_RES);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);

    build_panel();
    build_keys();
    lv_obj_set_hidden(s_root, true);
}

void ui_input_set_handler(ui_keypad_cb_t on_key, void *ctx)
{
    s_on_key = on_key;
    s_ctx = ctx;
}

void ui_input_set_text(const char *typed)
{
    lv_label_set_text(s_code, typed);
}

static void set_countdown_value(void *bar, int32_t value)
{
    lv_bar_set_value(bar, value, LV_ANIM_OFF);
}

void ui_input_set_visible(bool visible)
{
    if (!visible) {
        lv_anim_delete(s_countdown, set_countdown_value);  /* 不在畫面上就不必重畫 */
    }
    lv_obj_set_hidden(s_root, !visible);
}

void ui_input_restart_countdown(uint32_t timeout_ms)
{
    lv_anim_delete(s_countdown, set_countdown_value);
    lv_bar_set_range(s_countdown, 0, (int32_t)timeout_ms);
    lv_bar_set_value(s_countdown, (int32_t)timeout_ms, LV_ANIM_OFF);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, s_countdown);
    lv_anim_set_exec_cb(&anim, set_countdown_value);
    lv_anim_set_values(&anim, (int32_t)timeout_ms, 0);
    lv_anim_set_duration(&anim, timeout_ms);
    lv_anim_start(&anim);
}

void ui_input_set_mode(bool scanned)
{
    if (s_title == NULL || s_hint == NULL) {
        return;
    }
    lv_label_set_text(s_title, scanned ? "已讀取領球碼" : "請輸入領球碼");
    lv_label_set_text(s_hint, scanned ? "倒數結束會自動送出\n按「刪除」可以取消" : "倒數結束會回到待機");
}
