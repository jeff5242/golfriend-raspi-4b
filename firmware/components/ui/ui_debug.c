#include "ui_debug.h"

#include "sdkconfig.h"

#if CONFIG_GOLF_UI_DEBUG_CONSOLE

#include <stdio.h>

#include "driver/i2c_master.h"
#include "esp_cache.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/usb_serial_jtag_ll.h"
#include "lvgl.h"
#include "ui.h"
#include "ui_board.h"

static const char *TAG = "ui_debug";

#define DEBUG_POLL_MS       100
#define DEBUG_TASK_STACK    4096
#define DEBUG_RX_MAX         16
#define CAPTURE_STEP        2    /* 每 2 點取 1 點：512x300，傳輸量是全解析度的 1/4 */
#define CAPTURE_YIELD_ROWS  16   /* 每送幾行讓出 CPU 一次，避免看門狗 */
#define CAPTURE_LOCK_MS      1000
#define CAPTURE_REDRAW_MS    150
#define RENDER_SETTLE_MS    300  /* 切換畫面後等 LVGL 畫完 */

static esp_lcd_panel_handle_t s_panel;

/*
 * 把畫面記憶體送到序列埠。格式（tools/ui_capture.py 解析）：
 *   FBDUMP-BEGIN <寬> <高>
 *   每行：以空白分隔的 "rrrr" 或 "rrrr*n"（RGB565 十六進位，n 是連續相同點數）
 *   FBDUMP-END
 */
static void dump_frame_buffer(void);

static void capture(void)
{
    /*
     * 面板有兩張畫面記憶體輪流顯示。先強制重畫兩次，兩張內容就相同；
     * 再鎖住 LVGL 直到送完，避免動畫（倒數條）在傳輸中途重畫，抓到畫到一半的畫面。
     */
    for (int i = 0; i < 2; i++) {
        if (lvgl_port_lock(CAPTURE_LOCK_MS)) {
            lv_obj_invalidate(lv_screen_active());
            lvgl_port_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(CAPTURE_REDRAW_MS));
    }
    if (!lvgl_port_lock(CAPTURE_LOCK_MS)) {
        printf("FBDUMP-FAIL\n");
        return;
    }
    dump_frame_buffer();
    lvgl_port_unlock();
}

static void dump_frame_buffer(void)
{
    void *fb = NULL;
    void *fb_other = NULL;
    if (esp_lcd_dpi_panel_get_frame_buffer(s_panel, 2, &fb, &fb_other) != ESP_OK || fb == NULL) {
        printf("FBDUMP-FAIL\n");
        return;
    }
    /* 畫面記憶體由 DMA 寫入，先同步快取才讀得到最新內容 */
    const esp_err_t err = esp_cache_msync(fb, UI_LCD_H_RES * UI_LCD_V_RES * sizeof(uint16_t),
                                          ESP_CACHE_MSYNC_FLAG_DIR_M2C);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_cache_msync: %s", esp_err_to_name(err));
    }
    const uint16_t *px = fb;
    printf("FBDUMP-BEGIN %d %d\n", UI_LCD_H_RES / CAPTURE_STEP, UI_LCD_V_RES / CAPTURE_STEP);
    for (int y = 0; y < UI_LCD_V_RES; y += CAPTURE_STEP) {
        const uint16_t *row = px + y * UI_LCD_H_RES;
        int x = 0;
        while (x < UI_LCD_H_RES) {
            const uint16_t value = row[x];
            int run = 0;
            while (x < UI_LCD_H_RES && row[x] == value) {
                run++;
                x += CAPTURE_STEP;
            }
            if (run == 1) {
                printf("%04x ", value);
            } else {
                printf("%04x*%d ", value, run);
            }
        }
        printf("\n");
        if ((y / CAPTURE_STEP) % CAPTURE_YIELD_ROWS == 0) {
            vTaskDelay(1);
        }
    }
    printf("FBDUMP-END\n");
}

static void (*s_extra_handler)(int c);

void ui_set_debug_key_handler(void (*handler)(int c))
{
    s_extra_handler = handler;
}

/* 掃描觸控那條 I2C 匯流排，列出有回應的位址。用來確認鏡頭（OV02C10，0x36）有沒有接上。 */
static void probe_i2c(void)
{
    i2c_master_bus_handle_t bus = NULL;
    if (i2c_master_get_bus_handle(UI_TOUCH_I2C_PORT, &bus) != ESP_OK) {
        ESP_LOGW(TAG, "i2c bus not ready");
        return;
    }
    for (uint16_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe(bus, addr, 50) == ESP_OK) {
            ESP_LOGI(TAG, "i2c device at 0x%02x", addr);
        }
    }
    ESP_LOGI(TAG, "i2c scan done");
}

static void run_command(int c)
{
    switch (c) {
    case 'c': capture(); return;
    case 'p': probe_i2c(); return;
    case 's': ui_show_standby(); break;
    case 'i': ui_show_input("1234567", 10000); break;
    case 'r': ui_show_scanned("9557", 5000); break;
    case 'q': ui_show_querying(); break;
    case 'f': ui_show_confirm(50, 120, 70); break;
    case 'h': ui_show_confirm(50, 999999, 999999); break;
    case 'd': ui_show_dispensing(1, 3); break;
    case 'm': ui_show_menu("裝置 TEST_0001\n版本 P4-0.1.0\n網路 有線\nIP 192.168.1.20"); break;
    case 'u': ui_show_notice("系統更新中", "請勿關閉電源\n完成後會自動重新開機"); break;
    case 'e': ui_show_error("領球碼過期，請重新產生"); break;
    default:
        if (s_extra_handler != NULL && c > ' ') {
            s_extra_handler(c);
        }
        return;  /* 換行與空白略過 */
    }
    vTaskDelay(pdMS_TO_TICKS(RENDER_SETTLE_MS));
    printf("UIDEBUG-SHOWN %c\n", c);
}

static void debug_task(void *arg)
{
    (void)arg;
    ESP_LOGW(TAG, "debug console on: c=capture s/i/q/f/h/d/e/m=demo screens");
    for (;;) {
        /*
         * 直接讀 USB Serial/JTAG 的接收緩衝區：這個埠是第二主控台，只有輸出，getchar() 收不到。
         * 沒有安裝驅動程式，所以不影響 log 輸出。
         */
        uint8_t buf[DEBUG_RX_MAX];
        const int n = usb_serial_jtag_ll_read_rxfifo(buf, sizeof buf);
        for (int i = 0; i < n; i++) {
            run_command(buf[i]);
        }
        if (n == 0) {
            vTaskDelay(pdMS_TO_TICKS(DEBUG_POLL_MS));
        }
    }
}

void ui_debug_start(esp_lcd_panel_handle_t panel)
{
    s_panel = panel;
    if (xTaskCreate(debug_task, "ui_debug", DEBUG_TASK_STACK, NULL, 2, NULL) != pdPASS) {
        ESP_LOGE(TAG, "debug task not started");
    }
}

#else

void ui_debug_start(esp_lcd_panel_handle_t panel)
{
    (void)panel;
}

void ui_set_debug_key_handler(void (*handler)(int c))
{
    (void)handler;
}

#endif
