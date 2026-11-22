#include "ui_board.h"

#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_lcd_ek79007.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"

static const char *TAG = "ui_board";

#define BL_LEDC_TIMER   LEDC_TIMER_1
#define BL_LEDC_CHANNEL LEDC_CHANNEL_1
#define BL_DUTY_BITS    LEDC_TIMER_10_BIT
#define BL_DUTY_MAX     ((1 << 10) - 1)

static esp_err_t backlight_init(void)
{
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = BL_DUTY_BITS,
        .timer_num = BL_LEDC_TIMER,
        .freq_hz = 5000,
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

static esp_err_t panel_init(ui_board_t *out)
{
    esp_ldo_channel_handle_t ldo = NULL;
    const esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = UI_LCD_DSI_LDO_CHAN,
        .voltage_mv = UI_LCD_DSI_LDO_MV,
    };
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo_cfg, &ldo), TAG, "D-PHY LDO");

    esp_lcd_dsi_bus_handle_t dsi = NULL;
    const esp_lcd_dsi_bus_config_t bus_cfg = EK79007_PANEL_BUS_DSI_2CH_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&bus_cfg, &dsi), TAG, "DSI bus");

    const esp_lcd_dbi_io_config_t dbi_cfg = EK79007_PANEL_IO_DBI_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_dbi(dsi, &dbi_cfg, &out->io), TAG, "DBI io");

    /* 時序沿用驅動內建的 1024x600 60Hz；出廠程式用同一組驅動點亮這塊面板 */
    const esp_lcd_dpi_panel_config_t dpi_cfg =
        EK79007_1024_600_PANEL_60HZ_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
    ek79007_vendor_config_t vendor_cfg = {
        .mipi_config = {.dsi_bus = dsi, .dpi_config = &dpi_cfg, .lane_num = 2},
    };
    const esp_lcd_panel_dev_config_t dev_cfg = {
        .reset_gpio_num = UI_LCD_RST_GPIO,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_cfg,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_ek79007(out->io, &dev_cfg, &out->panel), TAG, "panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(out->panel), TAG, "panel reset");
    return esp_lcd_panel_init(out->panel);
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
    ESP_LOGI(TAG, "panel EK79007 %dx%d ready", UI_LCD_H_RES, UI_LCD_V_RES);

    const esp_err_t err = touch_init(&out->touch);
    if (err != ESP_OK) {
        /* 流程靠掃碼器，不靠觸控：觸控壞了照常開機 */
        ESP_LOGW(TAG, "touch GT911 not available: %s", esp_err_to_name(err));
        out->touch = NULL;
    }
    return ESP_OK;
}
