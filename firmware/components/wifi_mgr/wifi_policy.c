#include "wifi_policy.h"

#include <stddef.h>

void wifi_policy_init(wifi_policy_t *p, uint8_t ssid_count, uint8_t max_fail,
                      int64_t primary_retry_us)
{
    if (p == NULL) {
        return;
    }
    *p = (wifi_policy_t){
        .ssid_count = ssid_count == 0 ? 1 : ssid_count,
        .current = 0,
        .fail_count = 0,
        .max_fail = max_fail == 0 ? 1 : max_fail,
        .primary_retry_us = primary_retry_us,
        .connected_at_us = 0,
        .connected = false,
    };
}

uint8_t wifi_policy_on_fail(wifi_policy_t *p)
{
    if (p == NULL) {
        return 0;
    }
    p->connected = false;
    p->fail_count++;
    if (p->fail_count >= p->max_fail) {
        p->fail_count = 0;
        p->current = (uint8_t)((p->current + 1) % p->ssid_count);
    }
    return p->current;
}

void wifi_policy_on_connected(wifi_policy_t *p, int64_t now_us)
{
    if (p == NULL) {
        return;
    }
    p->connected = true;
    p->fail_count = 0;
    p->connected_at_us = now_us;
}

bool wifi_policy_should_retry_primary(const wifi_policy_t *p, int64_t now_us)
{
    if (p == NULL || !p->connected || p->current == 0) {
        return false;
    }
    return now_us - p->connected_at_us >= p->primary_retry_us;
}

void wifi_policy_select_primary(wifi_policy_t *p)
{
    if (p == NULL) {
        return;
    }
    p->current = 0;
    p->fail_count = 0;
    p->connected = false;
}
