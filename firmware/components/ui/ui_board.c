#include "ui_board.h"

#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_lcd_jd9165.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "ui_panel_jd9165_cmds.h"

static const char *TAG = "ui_board";

#define BL_LEDC_TIMER   LEDC_TIMER_1
#define BL_LEDC_CHANNEL LEDC_CHANNEL_1
#define BL_PWM_HZ       20000  /* 同出廠程式；高於人耳可聽範圍 */
#define BL_DUTY_BITS    LEDC_TIMER_10_BIT
#define BL_DUTY_MAX     ((1 << 10) - 1)

static esp_err_t backlight_init(void)
{
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = BL_DUTY_BITS,
        .timer_num = BL_LEDC_TIMER,
        .freq_hz = BL_PWM_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "backlight timer");
    const ledc_channel_config_t channel = {
        .gpio_num = UI_LCD_BL_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BL_LEDC_CHANNEL,
        .timer_sel = BL_LEDC_TIMER,
        .duty = 0,
    };
    return ledc_channel_config(&channel);
}

esp_err_t ui_board_set_backlight(int percent)
{
    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }
    ESP_RETURN_ON_ERROR(
        ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL, (uint32_t)(BL_DUTY_MAX * percent / 100)),
        TAG, "backlight duty");
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL);
}

/*
 * 面板是 JD9165。出廠程式的開機 log 寫「Install EK79007 LCD control panel」，
 * 那是 BSP 留下的字串；實際呼叫的是 esp_lcd_new_panel_jd9165()。
 * 下面的 DSI 速率、像素時脈、時序全部取自出廠程式（2026-11-22 從 flash 備份讀出）。
 * 用 EK79007 驅動時，面板沒有收到自己的設定：有背光，畫面時有時無。
 *
 * 重置腳位必須是 GPIO0（ui_board.h）。重置沒有作用時，面板暫存器會留著上一次開機寫的值，
 * 只有斷電才清得掉，畫面會偏色、出現直條紋。
 */
#define PANEL_DSI_LANES         2
#define PANEL_DSI_LANE_MBPS     550
#define PANEL_DPI_CLOCK_MHZ     52
#define PANEL_HSYNC_PULSE_WIDTH 24
#define PANEL_HSYNC_BACK_PORCH  136
#define PANEL_HSYNC_FRONT_PORCH 160
#define PANEL_VSYNC_PULSE_WIDTH 2
#define PANEL_VSYNC_BACK_PORCH  21
#define PANEL_VSYNC_FRONT_PORCH 12

static esp_err_t panel_init(ui_board_t *out)
{
    esp_ldo_channel_handle_t ldo = NULL;
    const esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = UI_LCD_DSI_LDO_CHAN,
        .voltage_mv = UI_LCD_DSI_LDO_MV,
    };
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo_cfg, &ldo), TAG, "D-PHY LDO");

    esp_lcd_dsi_bus_handle_t dsi = NULL;
    const esp_lcd_dsi_bus_config_t bus_cfg = {
        .bus_id = 0,
        .num_data_lanes = PANEL_DSI_LANES,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = PANEL_DSI_LANE_MBPS,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&bus_cfg, &dsi), TAG, "DSI bus");

    const esp_lcd_dbi_io_config_t dbi_cfg = JD9165_PANEL_IO_DBI_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_dbi(dsi, &dbi_cfg, &out->io), TAG, "DBI io");

    const esp_lcd_dpi_panel_config_t dpi_cfg = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = PANEL_DPI_CLOCK_MHZ,
        .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
        .num_fbs = 2,  /* 兩張畫面記憶體：畫完整張再切換，換頁不會看到畫到一半（出廠程式也是 2） */
        .video_timing = {
            .h_size = UI_LCD_H_RES,
            .v_size = UI_LCD_V_RES,
            .hsync_pulse_width = PANEL_HSYNC_PULSE_WIDTH,
            .hsync_back_porch = PANEL_HSYNC_BACK_PORCH,
            .hsync_front_porch = PANEL_HSYNC_FRONT_PORCH,
            .vsync_pulse_width = PANEL_VSYNC_PULSE_WIDTH,
            .vsync_back_porch = PANEL_VSYNC_BACK_PORCH,
            .vsync_front_porch = PANEL_VSYNC_FRONT_PORCH,
        },
        .flags = {.use_dma2d = true},
    };
    jd9165_vendor_config_t vendor_cfg = {
        .init_cmds = ui_panel_jd9165_cmds,
        .init_cmds_size = sizeof(ui_panel_jd9165_cmds) / sizeof(ui_panel_jd9165_cmds[0]),
        .mipi_config = {.dsi_bus = dsi, .dpi_config = &dpi_cfg},
    };
    const esp_lcd_panel_dev_config_t dev_cfg = {
        .reset_gpio_num = UI_LCD_RST_GPIO,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_cfg,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_jd9165(out->io, &dev_cfg, &out->panel), TAG, "panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(out->panel), TAG, "panel reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(out->panel), TAG, "panel init");
    return esp_lcd_panel_disp_on_off(out->panel, true);
}

static esp_err_t touch_init(esp_lcd_touch_handle_t *out)
{
    i2c_master_bus_handle_t bus = NULL;
    const i2c_master_bus_config_t bus_cfg = {
        .i2c_port = UI_TOUCH_I2C_PORT,
        .sda_io_num = UI_TOUCH_SDA_GPIO,
        .scl_io_num = UI_TOUCH_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &bus), TAG, "touch i2c bus");

    esp_lcd_panel_io_handle_t io = NULL;
    const esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(bus, &io_cfg, &io), TAG, "touch io");

    const esp_lcd_touch_config_t cfg = {
        .x_max = UI_LCD_H_RES,
        .y_max = UI_LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,  /* 這塊板子的觸控沒有接 RST、INT */
        .int_gpio_num = GPIO_NUM_NC,
    };
    return esp_lcd_touch_new_i2c_gt911(io, &cfg, out);
}

esp_err_t ui_board_init(ui_board_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = (ui_board_t){0};
    ESP_RETURN_ON_ERROR(backlight_init(), TAG, "backlight");
    ESP_RETURN_ON_ERROR(panel_init(out), TAG, "panel init");
    ESP_LOGI(TAG, "panel JD9165 %dx%d ready", UI_LCD_H_RES, UI_LCD_V_RES);

    const esp_err_t err = touch_init(&out->touch);
    if (err != ESP_OK) {
        /* 流程靠掃碼器，不靠觸控：觸控壞了照常開機 */
        ESP_LOGW(TAG, "touch GT911 not available: %s", esp_err_to_name(err));
        out->touch = NULL;
    }
    return ESP_OK;
}
