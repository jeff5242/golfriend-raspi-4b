#pragma once
/*
 * 輸入裝置：掃碼器、USB 鍵盤、ID 讀卡機。
 *
 * 三種裝置都整理成同一種輸出：一行字串（交易碼或卡號）。
 *   - HID 鍵盤模式（掃碼器、鍵盤、多數讀卡機）：USB Host + usb_host_hid
 *   - 序列埠模式的讀卡機：之後接 usb_host_cdc_acm 或 usb_host_ch34x_vcp，
 *     收到的位元組交給 scanner_handle_serial_bytes()
 * 同時接多個裝置時必須加 USB Hub（外接電源）。
 */
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "scanner_line.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 完成一行時呼叫（在 USB 工作中執行，回呼裡只能丟佇列，不得阻塞） */
typedef void (*scanner_line_cb_t)(const char *line, void *ctx);

/* 每收到一個可顯示字元時呼叫，給手動輸入畫面回顯用。可為 NULL。 */
typedef void (*scanner_key_cb_t)(char c, void *ctx);

/* 安裝 USB Host 與 HID 驅動，開始接收輸入。 */
esp_err_t scanner_start(scanner_line_cb_t on_line, scanner_key_cb_t on_key, void *ctx);

/* 處理一份 HID 開機協定鍵盤報告（8 bytes）。USB 回呼會呼叫；也方便測試。 */
void scanner_handle_keyboard_report(const uint8_t *report, size_t len);

/* 處理序列埠讀卡機送來的位元組（預留給 CDC／CH34x 驅動）。 */
void scanner_handle_serial_bytes(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif
