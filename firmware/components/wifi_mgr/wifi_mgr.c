#include "wifi_mgr.h"

#include <stdlib.h>
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
#define WIFI_HOLD_DELAY_US          (300LL * 1000)  /* 停掉連線嘗試後，等驅動處理完斷線再動作 */
#define WIFI_SCAN_START_TRIES       5    /* 驅動還在連線中時 scan_start 會失敗，隔一下再試 */
#define WIFI_SCAN_RAW_MAX           32   /* 一次最多讀幾筆掃描結果 */
#define WIFI_COLLECT_TIMEOUT_US     (10LL * 1000 * 1000)  /* 掃描完沒人來取結果：放棄，恢復重連 */

typedef struct {
    char ssid[33];
    char password[65];
} ap_entry_t;

/* 暫停自動重連之後要做的事 */
typedef enum {
    HOLD_NONE = 0,
    HOLD_SCAN,   /* 掃描附近的 WiFi */
    HOLD_APPLY,  /* 換成 s_staged 這組 SSID 重新連線 */
    HOLD_COLLECT, /* 掃描完成，等呼叫端用 wifi_mgr_scan_collect() 取結果 */
} hold_action_t;

static ap_entry_t s_aps[WIFI_MGR_MAX_AP];
static size_t s_ap_count;
static wifi_policy_t s_policy;
static volatile bool s_connected;
static wifi_mgr_status_cb_t s_on_status;
static void *s_ctx;
static esp_timer_handle_t s_reconnect_timer;
static esp_timer_handle_t s_primary_timer;
static volatile bool s_switching_to_primary;  /* 主動斷線去試主要 SSID，不算失敗 */
static volatile bool s_scanning_primary;      /* 正在掃描主要 SSID 在不在 */
static bool s_started;

/* WiFi 設定畫面：掃描與換網路。期間暫停自動重連，斷線不算失敗。 */
static esp_timer_handle_t s_hold_timer;
static volatile bool s_hold;
static hold_action_t s_hold_action;
static int s_scan_tries;
static volatile bool s_scanning_user;
static wifi_mgr_scan_cb_t s_scan_cb;
static void *s_scan_ctx;
static wifi_scan_ap_t s_results[WIFI_SCAN_LIST_MAX];
static size_t s_result_count;
static ap_entry_t s_staged[WIFI_MGR_MAX_AP];
static size_t s_staged_count;
static volatile int s_last_reason;
static bool s_collect_pending;  /* 掃描結果還在驅動裡；collect 與逾時誰先拿到誰處理 */

static void schedule_reconnect(void)
{
    if (s_ap_count == 0) {
        return;  /* 還沒有任何 SSID：等畫面設定 */
    }
    esp_timer_stop(s_reconnect_timer);
    esp_timer_start_once(s_reconnect_timer, WIFI_RECONNECT_DELAY_US);
}

static void apply_and_connect(uint8_t index)
{
    if (index >= s_ap_count) {
        return;
    }
    wifi_config_t cfg = {0};
    memcpy(cfg.sta.ssid, s_aps[index].ssid, sizeof cfg.sta.ssid);
    memcpy(cfg.sta.password, s_aps[index].password, sizeof cfg.sta.password);
    ESP_LOGI(TAG, "connecting to [%u] %s", (unsigned)index, s_aps[index].ssid);
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    const esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        /* 不會有 DISCONNECTED 事件接手重試，必須自己排下一次 */
        ESP_LOGW(TAG, "esp_wifi_connect: %s, retry later", esp_err_to_name(err));
        schedule_reconnect();
    }
}

static void on_reconnect_timer(void *arg)
{
    (void)arg;
    if (s_hold) {
        return;  /* 畫面正在掃描或換網路 */
    }
    apply_and_connect(s_policy.current);
}

/* 連在備援夠久：先掃描主要 SSID，看得到才斷線回去，不讓備援白白斷線 */
static void on_primary_timer(void *arg)
{
    (void)arg;
    const int64_t now = esp_timer_get_time();
    if (s_hold || s_scanning_primary || !wifi_policy_should_retry_primary(&s_policy, now)) {
        return;
    }
    wifi_scan_config_t scan = {.ssid = (uint8_t *)s_aps[0].ssid};
    const esp_err_t err = esp_wifi_scan_start(&scan, false);  /* 結果在 WIFI_EVENT_SCAN_DONE */
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "scan for primary failed: %s", esp_err_to_name(err));
        wifi_policy_on_primary_scan(&s_policy, false, now);
        return;
    }
    s_scanning_primary = true;
}

static void on_primary_scan_done(const wifi_event_sta_scan_done_t *done)
{
    s_scanning_primary = false;
    const bool seen = done != NULL && done->status == 0 && done->number > 0;
    esp_wifi_clear_ap_list();  /* 釋放掃描結果 */
    if (!wifi_policy_on_primary_scan(&s_policy, seen, esp_timer_get_time())) {
        ESP_LOGI(TAG, "primary %s %s, stay on backup", s_aps[0].ssid,
                 seen ? "seen but backup not connected" : "not found");
        return;
    }
    ESP_LOGI(TAG, "primary %s found, switching back", s_aps[0].ssid);
    s_switching_to_primary = true;
    const esp_err_t err = esp_wifi_disconnect();  /* 觸發 DISCONNECTED → 重連到主要 SSID */
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_disconnect: %s", esp_err_to_name(err));
        s_switching_to_primary = false;
    }
}

/* ---- WiFi 設定畫面：掃描與換網路 ---------------------------------------- */

/*
 * 暫停自動重連，WIFI_HOLD_DELAY_US 後在計時器裡做 action。
 * 驅動正在連線時不能掃描，所以沒連上時先停掉連線嘗試；換網路時一律先斷線。
 */
static void begin_hold(hold_action_t action)
{
    s_hold = true;
    s_hold_action = action;
    esp_timer_stop(s_reconnect_timer);
    if (action == HOLD_APPLY || !s_connected) {
        esp_wifi_disconnect();  /* 沒有在連線時會回錯誤，不影響 */
    }
    esp_timer_stop(s_hold_timer);
    esp_timer_start_once(s_hold_timer, WIFI_HOLD_DELAY_US);
}

static void end_hold(void)
{
    s_hold_action = HOLD_NONE;
    s_hold = false;
    if (!s_connected) {
        schedule_reconnect();
    }
}

static void finish_user_scan(bool ok)
{
    s_scanning_user = false;
    end_hold();
    if (s_scan_cb != NULL) {
        s_scan_cb(ok, s_scan_ctx);
    }
}

/*
 * 掃描完成（在 sys_evt 工作中執行）。這個工作的 stack 只有約 2.3 KB，
 * 讀結果要經由 C6 的 RPC、還要整理清單，放在這裡會 stack overflow（2026-11-23 實測當機）。
 * 所以這裡只通知，結果由呼叫端在自己的工作裡用 wifi_mgr_scan_collect() 取。
 */
static void on_user_scan_done(void)
{
    s_scanning_user = false;
    s_hold_action = HOLD_COLLECT;  /* 自動重連繼續暫停：重連會清掉驅動裡的掃描結果 */
    __atomic_store_n(&s_collect_pending, true, __ATOMIC_SEQ_CST);
    esp_timer_stop(s_hold_timer);
    esp_timer_start_once(s_hold_timer, WIFI_COLLECT_TIMEOUT_US);
    if (s_scan_cb != NULL) {
        s_scan_cb(true, s_scan_ctx);
    }
}

/* 讀出驅動裡的掃描結果存到 s_results。回傳 false 表示讀不到。 */
static bool read_scan_records(void)
{
    uint16_t number = WIFI_SCAN_RAW_MAX;
    wifi_ap_record_t *records = calloc(number, sizeof *records);
    if (records == NULL) {
        esp_wifi_clear_ap_list();
        return false;
    }
    const esp_err_t err = esp_wifi_scan_get_ap_records(&number, records);  /* 同時釋放驅動的結果 */
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "scan_get_ap_records: %s", esp_err_to_name(err));
        free(records);
        esp_wifi_clear_ap_list();
        return false;
    }
    static wifi_scan_ap_t raw[WIFI_SCAN_RAW_MAX];  /* 不放 stack；只有 collect 會用 */
    size_t count = 0;
    for (uint16_t i = 0; i < number && count < WIFI_SCAN_RAW_MAX; i++) {
        if (records[i].authmode == WIFI_AUTH_WPA2_ENTERPRISE) {
            continue;  /* 企業版認證要帳號，畫面只支援密碼 */
        }
        wifi_scan_ap_t *ap = &raw[count++];
        memcpy(ap->ssid, records[i].ssid, sizeof ap->ssid);
        ap->rssi = records[i].rssi;
        ap->secured = records[i].authmode != WIFI_AUTH_OPEN;
    }
    free(records);
    count = wifi_scan_list_normalize(raw, count, WIFI_SCAN_LIST_MAX);
    memcpy(s_results, raw, count * sizeof raw[0]);
    s_result_count = count;
    ESP_LOGI(TAG, "scan done: %u networks", (unsigned)count);
    return true;
}

static void on_hold_timer(void *arg)
{
    (void)arg;
    if (s_hold_action == HOLD_SCAN) {
        const wifi_scan_config_t scan = {.show_hidden = false};
        const esp_err_t err = esp_wifi_scan_start(&scan, false);  /* 結果在 WIFI_EVENT_SCAN_DONE */
        if (err == ESP_OK) {
            s_scanning_user = true;
            return;
        }
        if (++s_scan_tries < WIFI_SCAN_START_TRIES) {
            ESP_LOGW(TAG, "scan_start: %s, retry", esp_err_to_name(err));
            esp_timer_start_once(s_hold_timer, WIFI_HOLD_DELAY_US);
            return;
        }
        ESP_LOGE(TAG, "scan_start: %s, giving up", esp_err_to_name(err));
        finish_user_scan(false);
    } else if (s_hold_action == HOLD_COLLECT) {
        if (__atomic_exchange_n(&s_collect_pending, false, __ATOMIC_SEQ_CST)) {
            ESP_LOGW(TAG, "scan results not collected, dropping");
            esp_wifi_clear_ap_list();
            s_result_count = 0;
            end_hold();
        }
    } else if (s_hold_action == HOLD_APPLY) {
        memcpy(s_aps, s_staged, sizeof s_aps);
        s_ap_count = s_staged_count;
        wifi_policy_init(&s_policy, (uint8_t)s_ap_count, WIFI_MAX_FAIL_BEFORE_SWITCH,
                         WIFI_PRIMARY_RETRY_US);
        s_last_reason = 0;
        s_switching_to_primary = false;
        s_hold_action = HOLD_NONE;
        s_hold = false;
        ESP_LOGI(TAG, "ap list replaced (%u entries)", (unsigned)s_ap_count);
        apply_and_connect(0);
    }
}

/* ---- 事件 -------------------------------------------------------------- */

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
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        apply_and_connect(s_policy.current);  /* 沒有任何 SSID 時不做事 */
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
        if (s_scanning_user) {
            on_user_scan_done();
        } else if (s_scanning_primary) {
            on_primary_scan_done((const wifi_event_sta_scan_done_t *)data);
        }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const bool was_connected = s_connected;
        const wifi_event_sta_disconnected_t *info = data;
        if (s_hold) {
            /* 自己為了掃描或換網路停掉的連線：不算失敗、不重連，hold 結束後再接手 */
            ESP_LOGI(TAG, "disconnected while holding (reason=%d)",
                     info != NULL ? (int)info->reason : -1);
            if (was_connected) {
                notify(false);
            }
            return;
        }
        if (info != NULL) {
            s_last_reason = info->reason;
        }
        /* reason 對照 esp_wifi_types 的 wifi_err_reason_t，例：15 密碼錯、201 找不到 AP、8 自己離線 */
        ESP_LOGW(TAG, "disconnected from [%u] %s: reason=%d rssi=%d%s", (unsigned)s_policy.current,
                 s_ap_count > 0 ? s_aps[s_policy.current].ssid : "-",
                 info != NULL ? (int)info->reason : -1, info != NULL ? (int)info->rssi : 0,
                 s_switching_to_primary ? " (switching to primary)" : "");
        if (s_switching_to_primary) {
            s_switching_to_primary = false;  /* policy 已經選好主要 SSID */
        } else {
            wifi_policy_on_fail(&s_policy);
        }
        if (was_connected) {
            notify(false);
        }
        schedule_reconnect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        wifi_policy_on_connected(&s_policy, esp_timer_get_time());
        ESP_LOGI(TAG, "got IP on [%u] %s", (unsigned)s_policy.current,
                 s_ap_count > 0 ? s_aps[s_policy.current].ssid : "-");
        notify(true);
    }
}

/* 複製到 dst，略過空的 SSID。回傳實際筆數。 */
static size_t copy_aps(ap_entry_t *dst, const wifi_mgr_ap_t *aps, size_t count)
{
    size_t n = 0;
    memset(dst, 0, sizeof(ap_entry_t) * WIFI_MGR_MAX_AP);
    for (size_t i = 0; aps != NULL && i < count && n < WIFI_MGR_MAX_AP; i++) {
        if (aps[i].ssid == NULL || aps[i].ssid[0] == '\0') {
            continue;
        }
        ap_entry_t *e = &dst[n++];
        strlcpy(e->ssid, aps[i].ssid, sizeof e->ssid);
        strlcpy(e->password, aps[i].password != NULL ? aps[i].password : "", sizeof e->password);
    }
    return n;
}

esp_err_t wifi_mgr_start(const wifi_mgr_ap_t *aps, size_t count,
                         wifi_mgr_status_cb_t on_status, void *ctx)
{
    s_ap_count = copy_aps(s_aps, aps, count);
    if (s_ap_count == 0) {
        /* 照樣啟動：WiFi 設定畫面要能掃描 */
        ESP_LOGW(TAG, "no SSID configured, waiting for setup from screen");
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
    const esp_timer_create_args_t hold_args = {.callback = on_hold_timer, .name = "wifi_hold"};
    ESP_ERROR_CHECK(esp_timer_create(&reconnect_args, &s_reconnect_timer));
    ESP_ERROR_CHECK(esp_timer_create(&primary_args, &s_primary_timer));
    ESP_ERROR_CHECK(esp_timer_create(&hold_args, &s_hold_timer));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    /* 一律啟動：SSID 可能之後從畫面加到兩組以上；只有一組時 should_retry_primary 永遠是 false */
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_primary_timer, WIFI_PRIMARY_CHECK_US));
    s_started = true;
    return ESP_OK;
}

esp_err_t wifi_mgr_scan_start(wifi_mgr_scan_cb_t on_done, void *ctx)
{
    if (!s_started || s_hold || s_scanning_primary) {
        return ESP_ERR_INVALID_STATE;
    }
    s_scan_cb = on_done;
    s_scan_ctx = ctx;
    s_scan_tries = 0;
    begin_hold(HOLD_SCAN);
    return ESP_OK;
}

bool wifi_mgr_scan_collect(void)
{
    if (!__atomic_exchange_n(&s_collect_pending, false, __ATOMIC_SEQ_CST)) {
        return false;  /* 沒有待取的結果，或已經逾時丟掉 */
    }
    esp_timer_stop(s_hold_timer);
    const bool ok = read_scan_records();
    if (!ok) {
        s_result_count = 0;
    }
    end_hold();
    return ok;
}

size_t wifi_mgr_scan_results(wifi_scan_ap_t *out, size_t max)
{
    if (out == NULL) {
        return 0;
    }
    const size_t n = s_result_count < max ? s_result_count : max;
    memcpy(out, s_results, n * sizeof s_results[0]);
    return n;
}

esp_err_t wifi_mgr_set_aps(const wifi_mgr_ap_t *aps, size_t count)
{
    if (!s_started || s_hold_action == HOLD_SCAN || s_hold_action == HOLD_COLLECT) {
        return ESP_ERR_INVALID_STATE;
    }
    s_staged_count = copy_aps(s_staged, aps, count);
    begin_hold(HOLD_APPLY);
    return ESP_OK;
}

int wifi_mgr_last_reason(void)
{
    return s_last_reason;
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

const char *wifi_mgr_ssid(void)
{
    const int index = s_policy.current;
    if (!s_connected || index < 0 || index >= (int)s_ap_count) {
        return "";
    }
    return s_aps[index].ssid;
}
