#pragma once
/*
 * 目前用哪一條網路（只做判斷，不做 I/O）。純邏輯，可以在主機端測試。
 * 規則：有線有 IP 就用有線；有線沒有時用 WiFi。兩條線同時保持連線，互為備援。
 */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_NET_NONE = 0,
    APP_NET_ETH,
    APP_NET_WIFI,
} app_net_link_t;

app_net_link_t app_net_active(bool eth_up, bool wifi_up);

#ifdef __cplusplus
}
#endif
