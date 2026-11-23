#pragma once
/*
 * WiFi 設定畫面用的掃描結果整理與斷線原因分類。
 * 純邏輯，不依賴 ESP-IDF，可以在主機端測試。
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_SCAN_SSID_MAX 33  /* SSID 最長 32 bytes */
#define WIFI_SCAN_LIST_MAX 20  /* 畫面最多列幾個 */

typedef struct {
    char ssid[WIFI_SCAN_SSID_MAX];
    int8_t rssi;   /* dBm */
    bool secured;  /* 需要密碼 */
} wifi_scan_ap_t;

/*
 * 整理 aps[0..count)（就地修改）：
 *   - 去掉沒有名稱的（隱藏 SSID）
 *   - 同名的只留訊號最強的一筆（多台 AP 同名時畫面只列一次）
 *   - 依訊號由強到弱排序，最多留 max 筆
 * 回傳整理後的筆數。
 */
size_t wifi_scan_list_normalize(wifi_scan_ap_t *aps, size_t count, size_t max);

/* 斷線原因（wifi_err_reason_t）是不是密碼錯誤造成的 */
bool wifi_reason_is_auth_failure(int reason);

/* 斷線原因是不是找不到這個 SSID */
bool wifi_reason_is_not_found(int reason);

#ifdef __cplusplus
}
#endif
