#pragma once
/*
 * 有線網路（板上 IP101 PHY，RMII）。插上網路線並由 DHCP 拿到 IP 後算連線。
 * 有線的預設路由優先於 WiFi：兩條線都連上時，對外連線走有線；有線斷了自動改走 WiFi。
 */
#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 拿到 IP（connected=true）或失去連線（false）時呼叫。在事件工作中執行，不得阻塞。 */
typedef void (*eth_mgr_status_cb_t)(bool connected, void *ctx);

/* 啟動有線網路。沒插網路線不算錯誤，回傳 ESP_OK；板上沒有 PHY 時回傳錯誤。 */
esp_err_t eth_mgr_start(eth_mgr_status_cb_t on_status, void *ctx);

/* 有線網路有 IP 時回傳 true。 */
bool eth_mgr_is_connected(void);

#ifdef __cplusplus
}
#endif
