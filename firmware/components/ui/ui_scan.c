#include "ui_scan.h"

#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "ui.h"
#include "ui_theme.h"

static const char *TAG = "ui_scan";

#define SCAN_W            322
#define SCAN_H            182
#define SCAN_PAD          8
#define SCAN_LABEL_H      40
#define SCAN_MARGIN       16
#define SCAN_LOCK_MS      30   /* 拿不到鎖就跳過這一張，不得卡住鏡頭工作 */
#define SCAN_GUIDE_W_PCT  70   /* 對準框佔預覽的寬、高（%） */
#define SCAN_GUIDE_H_PCT  70
#define SCAN_GUIDE_COLOR  0x35D07F
#define SCAN_BLANK_GRAY   0xEE /* 還沒有影像時的底色 */
#define SCAN_BORDER_IDLE  0x9AA79F
#define SCAN_BORDER_FOUND 0xE0A100
#define SCAN_BORDER_READ  0x1F8A4C

static lv_obj_t *s_panel;
static lv_obj_t *s_image;
static lv_obj_t *s_label;
static uint8_t *s_pixels;
static lv_image_dsc_t s_dsc;
static volatile bool s_visible;
static ui_scan_status_t s_status = UI_SCAN_IDLE;

static void apply_status(void)
{
    const char *text = "把條碼放進框裡";
    uint32_t color = SCAN_BORDER_IDLE;
    if (s_status == UI_SCAN_FOUND) {
        text = "偵測到 QR Code，請拿穩";
        color = SCAN_BORDER_FOUND;
    } else if (s_status == UI_SCAN_TOO_CLOSE) {
        text = "請拿遠一點";
        color = SCAN_BORDER_FOUND;
    } else if (s_status == UI_SCAN_READ) {
        text = "已讀取";
        color = SCAN_BORDER_READ;
    }
    lv_label_set_text(s_label, text);
    lv_obj_set_style_border_color(s_panel, lv_color_hex(color), 0);
}

void ui_scan_build(lv_obj_t *parent)
{
    s_pixels = heap_caps_calloc(1, SCAN_W * SCAN_H, MALLOC_CAP_SPIRAM);
    if (s_pixels == NULL) {
        ESP_LOGW(TAG, "no memory for camera preview");
        return;
    }
    s_dsc = (lv_image_dsc_t){
        .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_L8,
                   .w = SCAN_W, .h = SCAN_H, .stride = SCAN_W},
        .data_size = SCAN_W * SCAN_H,
        .data = s_pixels,
    };

    s_panel = lv_obj_create(parent);
    lv_obj_remove_style_all(s_panel);
    lv_obj_set_size(s_panel, SCAN_W + SCAN_PAD * 2, SCAN_H + SCAN_LABEL_H + SCAN_PAD * 2);
    lv_obj_align(s_panel, LV_ALIGN_BOTTOM_RIGHT, -SCAN_MARGIN, -SCAN_MARGIN);
    lv_obj_set_style_bg_color(s_panel, lv_color_hex(UI_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(s_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_panel, 4, 0);
    lv_obj_set_style_radius(s_panel, 10, 0);
    lv_obj_set_clickable(s_panel, false);  /* 點預覽區等於點待機畫面 */
    lv_obj_remove_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);

    s_image = lv_image_create(s_panel);
    lv_image_set_src(s_image, &s_dsc);
    lv_obj_align(s_image, LV_ALIGN_TOP_MID, 0, SCAN_PAD);
    lv_obj_set_clickable(s_image, false);

    /* 對準框：條碼或 QR Code 整個放進框裡，距離就差不多對了 */
    lv_obj_t *guide = lv_obj_create(s_panel);
    lv_obj_remove_style_all(guide);
    lv_obj_set_size(guide, SCAN_W * SCAN_GUIDE_W_PCT / 100, SCAN_H * SCAN_GUIDE_H_PCT / 100);
    lv_obj_align_to(guide, s_image, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_border_width(guide, 3, 0);
    lv_obj_set_style_border_color(guide, lv_color_hex(SCAN_GUIDE_COLOR), 0);
    lv_obj_set_style_border_opa(guide, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(guide, 6, 0);
    lv_obj_set_clickable(guide, false);

    s_label = lv_label_create(s_panel);
    lv_obj_set_style_text_font(s_label, &ui_font_tc_24, 0);
    lv_obj_set_style_text_color(s_label, lv_color_hex(UI_COLOR_TEXT), 0);
    lv_obj_align(s_label, LV_ALIGN_BOTTOM_MID, 0, -SCAN_PAD);

    apply_status();
    lv_obj_set_hidden(s_panel, true);
}

void ui_scan_set_visible(bool visible)
{
    if (s_panel == NULL) {
        return;
    }
    if (visible && !s_visible) {
        /* 回到待機畫面：清掉上一次留下的影像與狀態，等鏡頭送來新的一張 */
        memset(s_pixels, SCAN_BLANK_GRAY, SCAN_W * SCAN_H);
        s_status = UI_SCAN_IDLE;
        apply_status();
        lv_obj_invalidate(s_image);
    }
    s_visible = visible;
    lv_obj_set_hidden(s_panel, !visible);
}

void ui_set_scan_status(ui_scan_status_t status)
{
    if (s_panel == NULL || status == s_status || !lvgl_port_lock(SCAN_LOCK_MS)) {
        return;
    }
    s_status = status;
    apply_status();
    lvgl_port_unlock();
}

void ui_set_scan_preview(const uint8_t *gray, int w, int h)
{
    if (s_panel == NULL || !s_visible || gray == NULL || w < SCAN_W || h < SCAN_H) {
        return;
    }
    const int step_x = w / SCAN_W;
    const int step_y = h / SCAN_H;
    if (!lvgl_port_lock(SCAN_LOCK_MS)) {
        return;
    }
    for (int y = 0; y < SCAN_H; y++) {
        const uint8_t *src = gray + (size_t)(y * step_y) * (size_t)w;
        uint8_t *dst = s_pixels + y * SCAN_W;
        for (int x = 0; x < SCAN_W; x++) {
            dst[SCAN_W - 1 - x] = src[x * step_x];  /* 左右相反：像照鏡子，客人比較容易對準 */
        }
    }
    lv_obj_invalidate(s_image);
    lvgl_port_unlock();
}
