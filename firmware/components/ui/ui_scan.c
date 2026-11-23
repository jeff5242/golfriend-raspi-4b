#include "ui_scan.h"

#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "ui.h"
#include "ui_theme.h"

static const char *TAG = "ui_scan";

/* 外框是螢幕的 1/4（寬、高各一半）。影像保持鏡頭的比例（644x364），放在外框上方置中 */
#define SCAN_PANEL_W      512
#define SCAN_PANEL_H      300
#define SCAN_W            432
#define SCAN_H            244
#define SCAN_PAD          8
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
static uint8_t *s_pixels;    /* 畫面上顯示的影像（LVGL 讀，必須持有鎖才能改） */
static uint8_t *s_back;      /* 鏡頭工作縮好的最新一張（只有鏡頭工作讀寫，不用鎖） */
static volatile bool s_back_fresh;  /* s_back 是這次進待機畫面之後的影像 */
static uint16_t s_xmap[SCAN_W];     /* 預覽第 x 欄取來源的哪一欄（已左右對調） */
static int s_xmap_w;                /* s_xmap 是依這個來源寬度算的 */
static lv_image_dsc_t s_dsc;
static volatile bool s_standby;  /* 在待機畫面：預覽影像持續更新（隱藏時也更新，出現時就是最新的） */
static volatile bool s_active;   /* 鏡頭畫面有動靜：預覽顯示出來 */
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
    s_back = heap_caps_calloc(1, SCAN_W * SCAN_H, MALLOC_CAP_SPIRAM);
    if (s_pixels == NULL || s_back == NULL) {
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
    lv_obj_set_size(s_panel, SCAN_PANEL_W, SCAN_PANEL_H);
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

/* 呼叫前必須已經拿到 lvgl_port_lock */
static void apply_visibility(void)
{
    lv_obj_set_hidden(s_panel, !(s_standby && s_active));
}

void ui_scan_set_visible(bool visible)
{
    if (s_panel == NULL) {
        return;
    }
    if (visible && !s_standby) {
        /* 回到待機畫面：清掉上一次留下的影像與狀態，等鏡頭送來新的一張 */
        memset(s_pixels, SCAN_BLANK_GRAY, SCAN_W * SCAN_H);
        s_back_fresh = false;
        s_status = UI_SCAN_IDLE;
        apply_status();
        lv_obj_invalidate(s_image);
    }
    s_standby = visible;
    apply_visibility();
}

void ui_set_scan_active(bool active)
{
    if (s_panel == NULL || active == s_active) {
        return;
    }
    ESP_LOGI(TAG, "preview %s", active ? "shown (motion)" : "hidden (no motion)");
    /* 一定要套用：不像預覽影像可以跳過一張，漏掉就會一直停在錯的顯示狀態 */
    if (!lvgl_port_lock(0)) {
        return;
    }
    s_active = active;
    if (active && s_back_fresh) {
        memcpy(s_pixels, s_back, SCAN_W * SCAN_H);  /* 一出現就是最新的一張 */
        lv_obj_invalidate(s_image);
    }
    apply_visibility();
    lvgl_port_unlock();
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

/*
 * 在鏡頭工作中呼叫。縮放在鎖外面做（寫到 s_back），持有 LVGL 鎖時只做一次整塊複製：
 * 縮放整段鎖著時每次約 10 ms、最長 110 ms，觸控會明顯變慢（2026-11-23 實測）。
 * 預覽隱藏時只更新 s_back，完全不碰 LVGL。
 */
void ui_set_scan_preview(const uint8_t *gray, int w, int h)
{
    if (s_panel == NULL || !s_standby || gray == NULL || w < SCAN_W || h < SCAN_H) {
        return;
    }
    if (w != s_xmap_w) {
        /* 依比例取最近的像素；左右相反：像照鏡子，客人比較容易對準 */
        for (int x = 0; x < SCAN_W; x++) {
            s_xmap[SCAN_W - 1 - x] = (uint16_t)(x * w / SCAN_W);
        }
        s_xmap_w = w;
    }
    for (int y = 0; y < SCAN_H; y++) {
        const uint8_t *src = gray + (size_t)(y * h / SCAN_H) * (size_t)w;
        uint8_t *dst = s_back + y * SCAN_W;
        for (int x = 0; x < SCAN_W; x++) {
            dst[x] = src[s_xmap[x]];
        }
    }
    s_back_fresh = true;
    if (!s_active) {
        return;  /* 隱藏中：出現時由 ui_set_scan_active 搬上去 */
    }
    if (!lvgl_port_lock(SCAN_LOCK_MS)) {
        return;
    }
    memcpy(s_pixels, s_back, SCAN_W * SCAN_H);
    lv_obj_invalidate(s_image);
    lvgl_port_unlock();
}
