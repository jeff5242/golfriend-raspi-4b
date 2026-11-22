#pragma once
/*
 * WiFi 管理：多組 SSID、斷線自動重連、連在備援時定期回頭試主要 SSID。
 * ESP32-P4 本身沒有 WiFi，透過板上的 ESP32-C6（esp_wifi_remote + esp_hosted）連線，
 * 程式介面和一般 esp_wifi 相同。
 */
#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_MGR_MAX_AP 3

typedef struct {
    const char *ssid;
    const char *password;
} wifi_mgr_ap_t;

/* 連上（connected=true）或斷線（false）時呼叫。ap_index 是目前使用的 SSID。 */
typedef void (*wifi_mgr_status_cb_t)(bool connected, int ap_index, void *ctx);

/* aps[0] 是主要 SSID。SSID 為空字串的項目會被略過。字串內容會被複製。 */
esp_err_t wifi_mgr_start(const wifi_mgr_ap_t *aps, size_t count,
                         wifi_mgr_status_cb_t on_status, void *ctx);

bool wifi_mgr_is_connected(void);

/* 目前連線的訊號強度（dBm）。沒連線時回傳 0。 */
int wifi_mgr_rssi(void);

/* 目前連線的 SSID。沒連線時回傳空字串。回傳的字串在程式執行期間一直有效。 */
const char *wifi_mgr_ssid(void);

#ifdef __cplusplus
}
#endif
