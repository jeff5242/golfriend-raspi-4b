#include "wifi_mgr.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "wifi_policy.h"

static const char *TAG = "wifi_mgr";

#define WIFI_MAX_FAIL_BEFORE_SWITCH 3
#define WIFI_RECONNECT_DELAY_US     (2LL * 1000 * 1000)
#define WIFI_PRIMARY_RETRY_US       (5LL * 60 * 1000 * 1000)  /* 連在備援 5 分鐘後回頭試主要 */
#define WIFI_PRIMARY_CHECK_US       (60LL * 1000 * 1000)

typedef struct {
    char ssid[33];
    char password[65];
} ap_entry_t;

static ap_entry_t s_aps[WIFI_MGR_MAX_AP];
static size_t s_ap_count;
static wifi_policy_t s_policy;
static volatile bool s_connected;
static wifi_mgr_status_cb_t s_on_status;
static void *s_ctx;
static esp_timer_handle_t s_reconnect_timer;
static esp_timer_handle_t s_primary_timer;
static volatile bool s_switching_to_primary;  /* 主動斷線去試主要 SSID，不算失敗 */

static void apply_and_connect(uint8_t index)
{
    wifi_config_t cfg = {0};
    memcpy(cfg.sta.ssid, s_aps[index].ssid, sizeof cfg.sta.ssid);
    memcpy(cfg.sta.password, s_aps[index].password, sizeof cfg.sta.password);
    ESP_LOGI(TAG, "connecting to [%u] %s", (unsigned)index, s_aps[index].ssid);
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_connect();
}

static void on_reconnect_timer(void *arg)
{
    (void)arg;
    apply_and_connect(s_policy.current);
}

static void on_primary_timer(void *arg)
{
    (void)arg;
    if (wifi_policy_should_retry_primary(&s_policy, esp_timer_get_time())) {
        ESP_LOGI(TAG, "on backup too long, retry primary");
        wifi_policy_select_primary(&s_policy);
        s_switching_to_primary = true;
        esp_wifi_disconnect();  /* 觸發 DISCONNECTED → 重連到主要 SSID */
    }
}

static void notify(bool connected)
{
    s_connected = connected;
    if (s_on_status != NULL) {
        s_on_status(connected, s_policy.current, s_ctx);
    }
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        apply_and_connect(s_policy.current);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const bool was_connected = s_connected;
        if (s_switching_to_primary) {
            s_switching_to_primary = false;  /* policy 已經選好主要 SSID */
        } else {
            wifi_policy_on_fail(&s_policy);
        }
        if (was_connected) {
            notify(false);
        }
        esp_timer_stop(s_reconnect_timer);
        esp_timer_start_once(s_reconnect_timer, WIFI_RECONNECT_DELAY_US);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        wifi_policy_on_connected(&s_policy, esp_timer_get_time());
        ESP_LOGI(TAG, "got IP on [%u] %s", (unsigned)s_policy.current,
                 s_aps[s_policy.current].ssid);
        notify(true);
    }
}

static esp_err_t copy_aps(const wifi_mgr_ap_t *aps, size_t count)
{
    s_ap_count = 0;
    for (size_t i = 0; i < count && s_ap_count < WIFI_MGR_MAX_AP; i++) {
        if (aps[i].ssid == NULL || aps[i].ssid[0] == '\0') {
            continue;
        }
        ap_entry_t *e = &s_aps[s_ap_count++];
        strlcpy(e->ssid, aps[i].ssid, sizeof e->ssid);
        strlcpy(e->password, aps[i].password != NULL ? aps[i].password : "", sizeof e->password);
    }
    return s_ap_count > 0 ? ESP_OK : ESP_ERR_INVALID_ARG;
}

esp_err_t wifi_mgr_start(const wifi_mgr_ap_t *aps, size_t count,
                         wifi_mgr_status_cb_t on_status, void *ctx)
{
    if (aps == NULL || copy_aps(aps, count) != ESP_OK) {
        ESP_LOGE(TAG, "no SSID configured (menuconfig: GOLF_WIFI1_SSID)");
        return ESP_ERR_INVALID_ARG;
    }
    s_on_status = on_status;
    s_ctx = ctx;
    wifi_policy_init(&s_policy, (uint8_t)s_ap_count, WIFI_MAX_FAIL_BEFORE_SWITCH,
                     WIFI_PRIMARY_RETRY_US);

    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {  /* 已建立過不算錯 */
        return err;
    }
    esp_netif_create_default_wifi_sta();

    const wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_event, NULL));

    const esp_timer_create_args_t reconnect_args = {.callback = on_reconnect_timer,
                                                    .name = "wifi_reconnect"};
    const esp_timer_create_args_t primary_args = {.callback = on_primary_timer,
                                                  .name = "wifi_primary"};
    ESP_ERROR_CHECK(esp_timer_create(&reconnect_args, &s_reconnect_timer));
    ESP_ERROR_CHECK(esp_timer_create(&primary_args, &s_primary_timer));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    if (s_ap_count > 1) {
        ESP_ERROR_CHECK(esp_timer_start_periodic(s_primary_timer, WIFI_PRIMARY_CHECK_US));
    }
    return ESP_OK;
}

bool wifi_mgr_is_connected(void)
{
    return s_connected;
}

int wifi_mgr_rssi(void)
{
    wifi_ap_record_t info;
    if (!s_connected || esp_wifi_sta_get_ap_info(&info) != ESP_OK) {
        return 0;
    }
    return info.rssi;
}
