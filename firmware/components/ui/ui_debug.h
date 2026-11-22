#pragma once
/*
 * 畫面除錯主控台（menuconfig：GOLF_UI_DEBUG_CONSOLE，預設關閉）。
 * 開啟後從板子的 USB Serial/JTAG 埠收單一字元指令：切換示範畫面、把畫面記憶體送回電腦（tools/ui_capture.py）。
 * 出貨韌體必須關閉：示範畫面會蓋掉真實流程的畫面。
 */
#include "esp_lcd_panel_ops.h"

/* 關閉時是空函式 */
void ui_debug_start(esp_lcd_panel_handle_t panel);
