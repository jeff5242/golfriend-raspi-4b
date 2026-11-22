#pragma once
/*
 * 板子：Guition JC1060P470（7 吋 1024x600 MIPI-DSI ＋ GT911 觸控）。
 * 腳位來源：廠商板子定義 github.com/Guition-JC/Guition
 *   components/board/guition_boards/jc1060p470/board_pins.h（2026-11-22 讀取）。
 * 換板子型號時只改這個檔案。
 */
#include "esp_err.h"
#include "esp_lcd_types.h"
#include "esp_lcd_touch.h"

#define UI_LCD_H_RES          1024
#define UI_LCD_V_RES          600
/* 重置是 GPIO0，不是廠商 board_pins.h 寫的 GPIO27。2026-11-22 實測：拉低 GPIO0 後面板
 * 暫存器回到預設值，拉低 GPIO27 沒有變化；出廠程式傳給驅動的也是 0。 */
#define UI_LCD_RST_GPIO       0
#define UI_LCD_BL_GPIO        23   /* 高電位亮 */
#define UI_LCD_DSI_LDO_CHAN   3    /* MIPI D-PHY 電源：P4 內建 LDO 第 3 路 */
#define UI_LCD_DSI_LDO_MV     2500
#define UI_TOUCH_I2C_PORT     0
#define UI_TOUCH_SDA_GPIO     7
#define UI_TOUCH_SCL_GPIO     8

typedef struct {
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_handle_t panel;
    esp_lcd_touch_handle_t touch;  /* 觸控初始化失敗時為 NULL，畫面照常顯示 */
} ui_board_t;

/* 開 D-PHY 電源、初始化 DSI 面板與觸控。背光保持關閉。 */
esp_err_t ui_board_init(ui_board_t *out);

/* 背光亮度 0–100。 */
esp_err_t ui_board_set_backlight(int percent);
