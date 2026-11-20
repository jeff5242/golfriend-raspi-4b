#include "ui.h"

#include <stddef.h>

#include "esp_log.h"

/*
 * TODO（原型步驟 0、5）：
 *   1. 板子型號確認後，加入廠商 BSP，初始化 MIPI-DSI 螢幕與 GT911 觸控。
 *   2. 用 esp_lvgl_port 建立 LVGL 顯示與輸入裝置。
 *   3. 用 lv_font_conv 把畫面用到的中文字轉成字型（Noto Sans TC 子集），放進 Flash。
 *   4. 待機圖用 JPEG 放 Flash 或 TF 卡，以硬體 JPEG 解碼。
 *   5. 狀態列格式沿用 Pi 版：ping 固定 3 碼、IP 用十六進位、不加 "IP:"。
 * 在那之前，所有畫面只印 log，方便先驗證流程。
 */

static const char *TAG = "ui";

esp_err_t ui_init(void)
{
    ESP_LOGW(TAG, "UI stub: display not initialised yet (board BSP pending)");
    return ESP_OK;
}

void ui_show_standby(void)
{
    ESP_LOGI(TAG, "[standby]");
}

void ui_show_input(const char *typed)
{
    ESP_LOGI(TAG, "[input] %s", typed != NULL ? typed : "");
}

void ui_show_querying(void)
{
    ESP_LOGI(TAG, "[querying]");
}

void ui_show_confirm(int take, long point_before, long point_after)
{
    ESP_LOGI(TAG, "[confirm] take=%d point %ld -> %ld", take, point_before, point_after);
}

void ui_show_dispensing(unsigned done, unsigned total)
{
    ESP_LOGI(TAG, "[dispensing] %u/%u", done, total);
}

void ui_show_error(const char *message)
{
    ESP_LOGW(TAG, "[error] %s", message != NULL ? message : "");
}

void ui_set_wifi_status(bool connected, int rssi_dbm)
{
    ESP_LOGI(TAG, "[wifi] %s %d dBm", connected ? "up" : "down", rssi_dbm);
}
