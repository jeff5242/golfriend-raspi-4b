#include "eth_mgr.h"

#include <stdatomic.h>

#include "esp_check.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

static const char *TAG = "eth_mgr";

/*
 * 腳位：RMII 與 MDC/MDIO 用 ESP-IDF 給 ESP32-P4 的預設值（MDC 31、MDIO 52、TX_EN 49、
 * TXD0 34、TXD1 35、CRS_DV 28、RXD0 29、RXD1 30、外部 50 MHz 時脈進 GPIO50）。
 * 廠商 board_pins.h 的值相同；IP101 驅動初始化時會從 MDIO 讀晶片 ID 驗證。
 */
#define ETH_PHY_RESET_GPIO 51
/* 預設路由優先度：WiFi station 是 100，數字大的優先 */
#define ETH_ROUTE_PRIO     200

static eth_mgr_status_cb_t s_on_status;
static void *s_ctx;
static atomic_bool s_connected;

static void set_connected(bool connected)
{
    if (atomic_exchange(&s_connected, connected) == connected) {
        return;
    }
    if (s_on_status != NULL) {
        s_on_status(connected, s_ctx);
    }
}

static void on_eth_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    (void)data;
    if (id == ETHERNET_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "link up, waiting for DHCP");
    } else if (id == ETHERNET_EVENT_DISCONNECTED) {
        ESP_LOGW(TAG, "link down");
        set_connected(false);
    }
}

static void on_ip_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    if (id == IP_EVENT_ETH_GOT_IP) {
        const ip_event_got_ip_t *event = data;
        ESP_LOGI(TAG, "got ip " IPSTR, IP2STR(&event->ip_info.ip));
        set_connected(true);
    } else if (id == IP_EVENT_ETH_LOST_IP) {
        ESP_LOGW(TAG, "lost ip");
        set_connected(false);
    }
}

static esp_err_t install_driver(esp_eth_handle_t *out)
{
    const eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    const eth_esp32_emac_config_t emac_cfg = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.phy_addr = ESP_ETH_PHY_ADDR_AUTO;
    phy_cfg.reset_gpio_num = ETH_PHY_RESET_GPIO;

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_cfg, &mac_cfg);
    ESP_RETURN_ON_FALSE(mac != NULL, ESP_FAIL, TAG, "mac");
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_cfg);
    if (phy == NULL) {
        mac->del(mac);
        ESP_LOGE(TAG, "phy");
        return ESP_FAIL;
    }
    const esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
    const esp_err_t err = esp_eth_driver_install(&eth_cfg, out);
    if (err != ESP_OK) {
        /* 找不到 PHY（晶片 ID 不符或沒有回應）：板子沒有有線網路，或腳位不同 */
        phy->del(phy);
        mac->del(mac);
        ESP_LOGE(TAG, "driver install: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t eth_mgr_start(eth_mgr_status_cb_t on_status, void *ctx)
{
    s_on_status = on_status;
    s_ctx = ctx;

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init");
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {  /* 已經建立過不算錯 */
        return err;
    }

    esp_eth_handle_t eth = NULL;
    ESP_RETURN_ON_ERROR(install_driver(&eth), TAG, "ethernet not available");

    esp_netif_inherent_config_t base = ESP_NETIF_INHERENT_DEFAULT_ETH();
    base.route_prio = ETH_ROUTE_PRIO;
    const esp_netif_config_t netif_cfg = {.base = &base, .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH};
    esp_netif_t *netif = esp_netif_new(&netif_cfg);
    ESP_RETURN_ON_FALSE(netif != NULL, ESP_ERR_NO_MEM, TAG, "netif");
    ESP_RETURN_ON_ERROR(esp_netif_attach(netif, esp_eth_new_netif_glue(eth)), TAG, "attach");

    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, on_eth_event, NULL), TAG, "event");
    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, on_ip_event, NULL), TAG, "event");
    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_LOST_IP, on_ip_event, NULL), TAG, "event");

    ESP_RETURN_ON_ERROR(esp_eth_start(eth), TAG, "start");
    ESP_LOGI(TAG, "started, route priority %d (wifi 100)", ETH_ROUTE_PRIO);
    return ESP_OK;
}

bool eth_mgr_is_connected(void)
{
    return atomic_load(&s_connected);
}
