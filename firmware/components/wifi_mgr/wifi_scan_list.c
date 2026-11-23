#include "wifi_scan_list.h"

#include <string.h>

/* 對照 esp_wifi_types 的 wifi_err_reason_t（寫成數字：這個檔案不依賴 ESP-IDF） */
#define REASON_AUTH_EXPIRE            2
#define REASON_4WAY_HANDSHAKE_TIMEOUT 15
#define REASON_NO_AP_FOUND            201
#define REASON_AUTH_FAIL              202
#define REASON_HANDSHAKE_TIMEOUT      204

size_t wifi_scan_list_normalize(wifi_scan_ap_t *aps, size_t count, size_t max)
{
    if (aps == NULL) {
        return 0;
    }
    size_t kept = 0;
    for (size_t i = 0; i < count; i++) {
        wifi_scan_ap_t ap = aps[i];
        ap.ssid[WIFI_SCAN_SSID_MAX - 1] = '\0';
        if (ap.ssid[0] == '\0') {
            continue;
        }
        size_t j = 0;
        while (j < kept && strcmp(aps[j].ssid, ap.ssid) != 0) {
            j++;
        }
        if (j < kept) {
            if (ap.rssi > aps[j].rssi) {
                aps[j] = ap;  /* 同名：留訊號強的 */
            }
            continue;
        }
        aps[kept++] = ap;
    }
    /* 插入排序：筆數少，而且同樣訊號時保持掃描順序 */
    for (size_t i = 1; i < kept; i++) {
        const wifi_scan_ap_t ap = aps[i];
        size_t j = i;
        while (j > 0 && aps[j - 1].rssi < ap.rssi) {
            aps[j] = aps[j - 1];
            j--;
        }
        aps[j] = ap;
    }
    return kept < max ? kept : max;
}

bool wifi_reason_is_auth_failure(int reason)
{
    return reason == REASON_AUTH_EXPIRE || reason == REASON_4WAY_HANDSHAKE_TIMEOUT ||
           reason == REASON_AUTH_FAIL || reason == REASON_HANDSHAKE_TIMEOUT;
}

bool wifi_reason_is_not_found(int reason)
{
    return reason == REASON_NO_AP_FOUND;
}
