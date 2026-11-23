#pragma once
/*
 * WiFi 管理：多組 SSID、斷線自動重連、連在備援時定期回頭試主要 SSID。
 * ESP32-P4 本身沒有 WiFi，透過板上的 ESP32-C6（esp_wifi_remote + esp_hosted）連線，
 * 程式介面和一般 esp_wifi 相同。
 */
#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "wifi_scan_list.h"

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

/*
 * aps[0] 是主要 SSID。SSID 為空字串的項目會被略過。字串內容會被複製。
 * 一組 SSID 都沒有時照樣啟動 WiFi（不連線），之後可以從畫面掃描、設定。
 */
esp_err_t wifi_mgr_start(const wifi_mgr_ap_t *aps, size_t count,
                         wifi_mgr_status_cb_t on_status, void *ctx);

/* 掃描完成時呼叫（ok=false 表示掃描失敗）。在 WiFi 事件或計時器工作中執行：只能丟佇列。 */
typedef void (*wifi_mgr_scan_cb_t)(bool ok, void *ctx);

/*
 * 掃描附近的 WiFi（WiFi 設定畫面用）。掃描期間暫停自動重連。
 * on_done(true) 之後必須呼叫 wifi_mgr_scan_collect()，自動重連才會恢復（10 秒沒呼叫就放棄結果、恢復重連）。
 * 已經在掃描或正在換網路時回傳 ESP_ERR_INVALID_STATE。
 */
esp_err_t wifi_mgr_scan_start(wifi_mgr_scan_cb_t on_done, void *ctx);

/*
 * 讀出掃描結果並恢復自動重連。會經由 C6 的 RPC 讀資料，必須在 stack 夠大的工作呼叫（例如主迴圈），
 * 不得在 on_done 回呼裡呼叫。沒有待取的結果或讀取失敗時回傳 false。
 */
bool wifi_mgr_scan_collect(void);

/* 上一次 wifi_mgr_scan_collect() 取到的結果（已依訊號排序、去掉重複），回傳筆數。 */
size_t wifi_mgr_scan_results(wifi_scan_ap_t *out, size_t max);

/*
 * 換掉整組 SSID，從 aps[0] 開始重新連線（已連線時會先斷線）。字串內容會被複製。
 * 結果和平常一樣由 on_status 通知。
 */
esp_err_t wifi_mgr_set_aps(const wifi_mgr_ap_t *aps, size_t count);

/* 最近一次斷線的原因（wifi_err_reason_t）。換網路之後還沒斷過線時是 0。 */
int wifi_mgr_last_reason(void);

bool wifi_mgr_is_connected(void);

/* 目前連線的訊號強度（dBm）。沒連線時回傳 0。 */
int wifi_mgr_rssi(void);

/* 目前連線的 SSID。沒連線時回傳空字串。回傳的字串在程式執行期間一直有效。 */
const char *wifi_mgr_ssid(void);

#ifdef __cplusplus
}
#endif
