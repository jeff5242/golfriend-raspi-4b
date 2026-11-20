#pragma once
/*
 * WiFi 多組 SSID 的切換規則。
 * 純邏輯，不依賴 ESP-IDF，可以在主機端測試。
 *
 * 規則：
 *   - index 0 是主要 SSID（例：golfriend），後面依序是備援（例：HUAWEI）。
 *   - 同一組連續失敗 max_fail 次後，換下一組；最後一組失敗後回到第 0 組。
 *   - 連上任何一組後，失敗次數歸零。
 *   - 連在備援上時，每 primary_retry_us 回頭試一次主要 SSID。
 */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t ssid_count;
    uint8_t current;
    uint8_t fail_count;
    uint8_t max_fail;
    int64_t primary_retry_us;
    int64_t connected_at_us;
    bool connected;
} wifi_policy_t;

void wifi_policy_init(wifi_policy_t *p, uint8_t ssid_count, uint8_t max_fail,
                      int64_t primary_retry_us);

/* 連線失敗或斷線時呼叫。回傳下一次要試的 SSID index。 */
uint8_t wifi_policy_on_fail(wifi_policy_t *p);

/* 連上 current 時呼叫。 */
void wifi_policy_on_connected(wifi_policy_t *p, int64_t now_us);

/* 連在備援上、且已超過 primary_retry_us 時回傳 true，表示該回頭試主要 SSID。 */
bool wifi_policy_should_retry_primary(const wifi_policy_t *p, int64_t now_us);

/* 回頭試主要 SSID 前呼叫：current 設回 0，失敗次數歸零。 */
void wifi_policy_select_primary(wifi_policy_t *p);

#ifdef __cplusplus
}
#endif
