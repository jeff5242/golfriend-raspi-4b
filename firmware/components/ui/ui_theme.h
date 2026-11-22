#pragma once
/*
 * 畫面共用的顏色與字型（白底、綠色重點，和待機圖同一套）。
 * 字型由 tools/gen_ui_font.py 產生，只收錄字串用到的字。
 */
#include "lvgl.h"

LV_FONT_DECLARE(ui_font_tc_24)
LV_FONT_DECLARE(ui_font_tc_32)
LV_FONT_DECLARE(ui_font_tc_56)

#define UI_COLOR_BG          0xFFFFFF
#define UI_COLOR_PANEL       0xF6F8F5  /* 輸入畫面左欄 */
#define UI_COLOR_LINE        0xE3E8E4
#define UI_COLOR_TEXT        0x1D2A23
#define UI_COLOR_TEXT_MUTED  0x66726B
#define UI_COLOR_ACCENT      0x1F8A4C  /* 綠：確認、進度、輸入底線 */
#define UI_COLOR_ACCENT_DOWN 0x176B3A
#define UI_COLOR_ERROR       0x9C2F22
#define UI_COLOR_KEY         0xEEF2EE
#define UI_COLOR_KEY_DOWN    0xC9E5D3
#define UI_COLOR_KEY_BORDER  0x9AA79F  /* 按鍵框線：白底上要看得出每顆鍵的範圍 */
#define UI_KEY_BORDER_WIDTH  2
#define UI_COLOR_DELETE      0xFBE4E0  /* 淡紅：刪除 */
#define UI_COLOR_DELETE_DOWN 0xF2C2BA
#define UI_COLOR_DELETE_TEXT 0x9C2F22
#define UI_RADIUS_KEY        14
