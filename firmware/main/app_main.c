/*
 * Golfriend 出球機 ESP32-P4 原型（POC）主程式。
 *
 * 流程沿用 Pi 版 MediaPlayer.py：
 *   掃碼／讀卡／鍵盤輸入 → take_ball → 2 秒後 take_ball_confirm → 出球 → 回待機
 *
 * 所有輸入（掃碼器、讀卡機、WiFi、馬達完成）都變成 app_msg_t 丟進同一個佇列，
 * 由 app_main 的迴圈依序處理，狀態轉換規則在 app_fsm.c（有主機端測試）。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "api.h"
#include "api_errors.h"
#include "app_dispense.h"
#include "app_fsm.h"
#include "app_net.h"
#include "app_typed.h"
#include "camera.h"
#include "qr_code.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "eth_mgr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "motor.h"
#include "motor_plan.h"
#include "nvs_flash.h"
#include "ota.h"
#include "remote_log.h"
#include "scanner.h"
#include "settings.h"
#include "standby_store.h"
#include "tfcard.h"
#include "sdkconfig.h"
#include "ui.h"
#include "wifi_mgr.h"

static const char *TAG = "app";

/* 遠端 log 的簡寫：系統／操作／例外，對應 Pi 版 Log.system()／operation()／exception() */
#define RLOG_SYSTEM(...)    remote_log_event(REMOTE_LOG_SYSTEM, "INFO", __VA_ARGS__)
#define RLOG_OPERATION(...) remote_log_event(REMOTE_LOG_OPERATION, "INFO", __VA_ARGS__)
#define RLOG_EXCEPTION(...) remote_log_event(REMOTE_LOG_EXCEPTION, "ERROR", __VA_ARGS__)

#define INPUT_TIMEOUT_MS 10000  /* 同 Pi 版 TakeBallDialog 10 秒 */
#define CONFIRM_DELAY_MS 2000   /* 同 Pi 版 QR 路徑 2 秒自動確認 */
#define ERROR_SHOW_MS    3000
#define MENU_TIMEOUT_MS   30000  /* 系統選單沒有操作 30 秒後回待機 */
#define MAINT_CODE        "7777777"  /* 同 Pi 版的維修碼；Pi 版是關機，MCU 版開系統選單 */
#define MENU_REBOOT_TEXT  "reboot"
#define MENU_UPDATE_TEXT  "update"
#define OTA_TASK_STACK    8192
#define OTA_URL_MAX       200
#define OTA_VERSION_MAX   24
#define OTA_ENTER_WAIT_MS 2000   /* 等流程進入「更新中」；進不去代表有人正在領球 */
#define OTA_BUSY_RETRY_MS 600000 /* 有人在領球時，10 分鐘後再試 */
#define OTA_WAIT_PING_MS  5000
#define MENU_INFO_MAX     192
#define CONFIG_FILE_PATH  TFCARD_MOUNT_POINT "/config.txt"
#define STANDBY_FILE_PATH TFCARD_MOUNT_POINT "/standby.jpg"
#define NOTICE_MAX        128
#define NOTICE_SHOW_MS    3000
#define REBOOT_FLUSH_MS   7000  /* 遠端 log 每 5 秒送一批 */
#define APP_QUEUE_LEN    16
#define ERROR_TEXT_MAX   64
#define API_CODE_MAX     16
#define PING_NOTE_MAX    64
#define PING_RETRY_MS    5000   /* 還沒有網路或還沒對時：稍後再試 */
#define PING_TASK_STACK  6144

typedef struct {
    app_event_t event;
    char text[SCANNER_LINE_MAX];
} app_msg_t;

static QueueHandle_t s_queue;
static app_state_t s_state = APP_ST_STANDBY;
static char s_typed[SCANNER_LINE_MAX];
static char s_code[SCANNER_LINE_MAX];
static api_take_ball_result_t s_result;
static motor_plan_t s_plan;
static char s_error[ERROR_TEXT_MAX];
static TaskHandle_t s_ota_task;
static volatile bool s_backend_ok;     /* 這次開機有沒有成功回報過後台 */
static volatile bool s_ota_manual;     /* 從系統選單按「檢查更新」：結果要顯示在畫面上 */
static settings_t s_settings;  /* 開機時讀一次；改設定後重開機生效 */

/* ---- 回呼：只丟佇列，不做事 ------------------------------------------- */

static void post(app_event_t event, const char *text)
{
    app_msg_t msg = {.event = event};
    if (text != NULL) {
        strlcpy(msg.text, text, sizeof msg.text);
    }
    if (xQueueSend(s_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "queue full, drop event %d", (int)event);
    }
}

static void on_scanner_line(const char *line, void *ctx)
{
    (void)ctx;
    post(APP_EV_LINE, line);
}

/* 鏡頭讀到 QR Code：和掃碼器送來一整行相同，走同一條流程 */
static void on_camera_code(const char *code, void *ctx)
{
    (void)ctx;
    post(APP_EV_LINE, code);
}

/* 鏡頭的掃描狀態與預覽，轉給畫面。qr_scan_state_t 和 ui_scan_status_t 的順序相同。 */
static void on_camera_status(int status, void *ctx)
{
    (void)ctx;
    ui_set_scan_status(status == QR_SCAN_READ    ? UI_SCAN_READ
                       : status == QR_SCAN_FOUND ? UI_SCAN_FOUND
                                                 : UI_SCAN_IDLE);
}

static void on_camera_preview(const uint8_t *gray, int w, int h, void *ctx)
{
    (void)ctx;
    ui_set_scan_preview(gray, w, h);
}

/* 除錯主控台的鏡頭指令：v = 把下一張鏡頭影像送回電腦 */
static void on_debug_key(int c)
{
    if (c == 'v') {
        camera_debug_dump_next();
    }
}

static void on_scanner_key(char c, void *ctx)
{
    (void)ctx;
    const char s[2] = {c, '\0'};
    post(APP_EV_KEY, s);
}

/* 觸控回呼在 LVGL 工作中執行，同樣只丟佇列 */
static void on_ui_touch(void *ctx)
{
    (void)ctx;
    post(APP_EV_TOUCH, NULL);
}

static void on_ui_key(char c, void *ctx)
{
    on_scanner_key(c, ctx);
}

static void on_ui_menu(ui_menu_action_t action, void *ctx)
{
    (void)ctx;
    post(APP_EV_MENU_CLOSE, action == UI_MENU_REBOOT   ? MENU_REBOOT_TEXT
                            : action == UI_MENU_UPDATE ? MENU_UPDATE_TEXT
                                                       : NULL);
}

static void on_motor_done(esp_err_t result, void *ctx)
{
    (void)ctx;
    if (result != ESP_OK) {
        post(APP_EV_DISPENSE_FAIL, "出球中斷，請洽櫃台");  /* 交易已確認，球沒出完 */
        return;
    }
    RLOG_OPERATION("dispense done");
    post(APP_EV_MOTOR_DONE, NULL);
}

static app_net_link_t active_link(void)
{
    return app_net_active(eth_mgr_is_connected(), wifi_mgr_is_connected());
}

/* 有線或 WiFi 的連線狀態變了：第一次有網路時開始對時，更新狀態列 */
static void on_net_changed(void)
{
    static bool sntp_started = false;
    const app_net_link_t link = active_link();
    if (link != APP_NET_NONE && !sntp_started) {
        sntp_started = api_time_sync_start() == ESP_OK;  /* 失敗時下次連上再試 */
    }
    ui_set_lan_status(link == APP_NET_ETH);
    ui_set_wifi_status(wifi_mgr_is_connected(), wifi_mgr_ssid(), wifi_mgr_rssi());
}

static void on_wifi_status(bool connected, int ap_index, void *ctx)
{
    (void)ctx;
    ESP_LOGI(TAG, "wifi %s (ap %d)", connected ? "up" : "down", ap_index);
    RLOG_SYSTEM("wifi %s (ap %d, rssi %d)", connected ? "up" : "down", ap_index, wifi_mgr_rssi());
    on_net_changed();
}

static void on_eth_status(bool connected, void *ctx)
{
    (void)ctx;
    ESP_LOGI(TAG, "ethernet %s", connected ? "up" : "down");
    RLOG_SYSTEM("ethernet %s", connected ? "up" : "down");
    on_net_changed();
}

/* ---- 定期回報 ---------------------------------------------------------- */

/* 目前對外連線用的 IP（有線優先）。沒有網路時回傳 false。 */
static bool active_ip(char *out, size_t out_size)
{
    esp_netif_ip_info_t info;
    esp_netif_t *netif = esp_netif_get_default_netif();
    if (netif == NULL || esp_netif_get_ip_info(netif, &info) != ESP_OK || info.ip.addr == 0) {
        return false;
    }
    snprintf(out, out_size, IPSTR, IP2STR(&info.ip));
    return true;
}

/*
 * 定期送 take_ball_ping，後台用它更新裝置時間與上線狀態。
 * 獨立工作：HTTP 最多等 10 秒，不得卡住領球流程。
 * Pi 版 V2.6 的回報程式沒有被呼叫（舊版是每播完一段影片送一次），所以間隔由 menuconfig 決定。
 */
static void ping_task(void *arg)
{
    (void)arg;
    for (;;) {
        char ip[16];
        if (active_link() == APP_NET_NONE || !api_time_is_valid() || !active_ip(ip, sizeof ip)) {
            vTaskDelay(pdMS_TO_TICKS(PING_RETRY_MS));
            continue;
        }
        char note[PING_NOTE_MAX];
        char code[API_CODE_MAX];
        snprintf(note, sizeof note, "%s_%s", ip, esp_app_get_description()->version);
        const esp_err_t err = api_ping(note, code, sizeof code);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "ping ok: %s", note);
            /* 新版韌體第一次成功連到後台：確認這一版正常，取消退回 */
            if (ota_is_pending_verify()) {
                ota_confirm_running();
                RLOG_SYSTEM("ota: version %s confirmed", esp_app_get_description()->version);
            }
            s_backend_ok = true;
        } else {
            ESP_LOGW(TAG, "ping failed: %s code=%s", esp_err_to_name(err), code);
        }
        vTaskDelay(pdMS_TO_TICKS(CONFIG_GOLF_PING_INTERVAL_S * 1000));
    }
}

/* ---- 韌體遠端更新 ------------------------------------------------------ */

static void ota_show_result(const char *message)
{
    if (!s_ota_manual) {
        return;
    }
    ui_show_notice("韌體更新", message);
    vTaskDelay(pdMS_TO_TICKS(NOTICE_SHOW_MS));
    if (s_state == APP_ST_STANDBY) {
        ui_show_standby();
    }
}

/* 檢查一次；有新版而且機器閒著就更新。回傳下次檢查前要等多久。 */
static TickType_t ota_run_once(void)
{
    const TickType_t normal = pdMS_TO_TICKS(CONFIG_GOLF_OTA_CHECK_INTERVAL_H * 3600 * 1000);
    char url[OTA_URL_MAX];
    char version[OTA_VERSION_MAX];
    if (active_link() == APP_NET_NONE) {
        ota_show_result("沒有網路");
        return normal;
    }
    const esp_err_t check = ota_check(s_settings.ota_url, url, sizeof url, version, sizeof version);
    if (check != ESP_OK) {
        ota_show_result(check == ESP_ERR_NOT_FOUND ? "已是最新版本" : "無法檢查更新");
        return normal;
    }
    post(APP_EV_UPDATE_START, version);
    vTaskDelay(pdMS_TO_TICKS(OTA_ENTER_WAIT_MS));
    if (s_state != APP_ST_UPDATE) {
        ESP_LOGI(TAG, "ota: machine busy, retry later");
        return pdMS_TO_TICKS(OTA_BUSY_RETRY_MS);
    }
    RLOG_SYSTEM("ota: downloading %s", version);
    if (ota_download(url) != ESP_OK) {
        RLOG_EXCEPTION("ota: download of %s failed", version);
        post(APP_EV_UPDATE_END, NULL);
        return normal;
    }
    RLOG_SYSTEM("ota: %s downloaded, rebooting", version);
    vTaskDelay(pdMS_TO_TICKS(REBOOT_FLUSH_MS));
    esp_restart();
    return normal;
}

/*
 * 開機後等第一次成功回報後台才開始檢查（確定網路與時間都正常）。
 * 之後每隔 GOLF_OTA_CHECK_INTERVAL_H 小時檢查一次；系統選單的「檢查更新」會立刻叫醒它。
 */
static void ota_task(void *arg)
{
    (void)arg;
    while (!s_backend_ok) {
        vTaskDelay(pdMS_TO_TICKS(OTA_WAIT_PING_MS));
    }
    for (;;) {
        const TickType_t wait = ota_run_once();
        s_ota_manual = false;
        if (ulTaskNotifyTake(pdTRUE, wait) > 0) {
            s_ota_manual = true;
        }
    }
}

/* ---- 各狀態要做的事 ---------------------------------------------------- */

static void set_error(const char *text)
{
    strlcpy(s_error, text, sizeof s_error);
}

#ifdef CONFIG_GOLF_TEST_CONFIRM_WITHOUT_MOTOR
#define CONFIRM_WITHOUT_MOTOR true
#else
#define CONFIRM_WITHOUT_MOTOR false
#endif
#define CONFIRM_ONLY_NOTICE_MS 4000

static bool s_confirm_only;  /* 這一筆是測試模式：已送確認，不出球 */

/* 送確認（扣點）之前先算好時序、確認馬達可用。出不了球就不扣點。 */
static bool prepare_dispense(void)
{
    const motor_params_t params = {
        .type = s_result.type,
        .take = s_result.take,
        .timer_s = s_result.timer_s,
        .start_timer_s = s_result.start_timer_s,
        .interval_timer_s = s_result.interval_timer_s,
        .ball_per_plate = s_result.ball_per_plate,
    };
    const app_dispense_check_t check =
        app_dispense_prepare(&params, motor_is_ready(), CONFIRM_WITHOUT_MOTOR, &s_plan);
    s_confirm_only = check == APP_DISPENSE_CONFIRM_ONLY;
    if (s_confirm_only) {
        ESP_LOGW(TAG, "TEST MODE: confirming id=%s without motor (points will be deducted)",
                 s_result.id);
        return true;
    }
    if (check == APP_DISPENSE_OK) {
        return true;
    }
    ESP_LOGE(TAG, "not confirming id=%s: check=%d type=%d take=%d", s_result.id, (int)check,
             s_result.type, s_result.take);
    RLOG_EXCEPTION("not confirming id=%s: check=%d type=%d take=%d", s_result.id, (int)check,
                   s_result.type, s_result.take);
    set_error(check == APP_DISPENSE_MOTOR_NOT_READY ? "出球機未就緒，請洽櫃台"
                                                    : "出球設定錯誤，請洽櫃台");
    return false;
}

static app_event_t do_query(void)
{
    if (active_link() == APP_NET_NONE) {
        set_error("網路連接錯誤");
        return APP_EV_API_FAIL;
    }
    char code[API_CODE_MAX];
    const esp_err_t err = api_take_ball(s_code, &s_result, code, sizeof code);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "take_ball ok: id=%s take=%d type=%d", s_result.id, s_result.take,
                 s_result.type);
        RLOG_OPERATION("take_ball ok: code=%s id=%s take=%d type=%d", s_code, s_result.id,
                       s_result.take, s_result.type);
        return prepare_dispense() ? APP_EV_API_OK : APP_EV_API_FAIL;
    }
    /* 螢幕還沒接上時，只能從 log 看伺服器回了什麼錯誤代碼 */
    ESP_LOGW(TAG, "take_ball failed: %s code=%s", esp_err_to_name(err), code);
    RLOG_EXCEPTION("take_ball failed: code=%s err=%s error_msg=%s", s_code, esp_err_to_name(err),
                   code);
    set_error(api_failure_text(API_CMD_TAKE_BALL, err == ESP_FAIL, code));
    return APP_EV_API_FAIL;
}

static app_event_t do_confirm(void)
{
    vTaskDelay(pdMS_TO_TICKS(CONFIRM_DELAY_MS));
    char note[24];
    char code[API_CODE_MAX];
    snprintf(note, sizeof note, "takeBall%d", s_result.take);  /* 同 Pi 版 note 格式 */
    const esp_err_t err = api_take_ball_confirm(s_result.id, note, "1", code, sizeof code);
    if (err == ESP_OK) {
        RLOG_OPERATION("take_ball_confirm ok: id=%s take=%d", s_result.id, s_result.take);
        return APP_EV_CONFIRM_OK;
    }
    RLOG_EXCEPTION("take_ball_confirm failed: id=%s err=%s error_msg=%s", s_result.id,
                   esp_err_to_name(err), code);
    ESP_LOGW(TAG, "take_ball_confirm failed: %s code=%s id=%s", esp_err_to_name(err), code,
             s_result.id);
    set_error(api_failure_text(API_CMD_TAKE_BALL_CONFIRM, err == ESP_FAIL, code));
    return APP_EV_API_FAIL;
}

/* 回傳 true 表示馬達已開始跑，之後等 APP_EV_MOTOR_DONE。時序在 prepare_dispense() 算好。 */
static bool do_dispense(void)
{
    const esp_err_t err = motor_run(&s_plan, on_motor_done, NULL);
    if (err != ESP_OK) {
        /* 交易已確認但無法出球：記錄交易編號，畫面請客人洽櫃台 */
        ESP_LOGE(TAG, "cannot dispense: %s id=%s take=%d", esp_err_to_name(err), s_result.id,
                 s_result.take);
        set_error("出球失敗，請洽櫃台");
        RLOG_EXCEPTION("cannot dispense: %s id=%s take=%d", esp_err_to_name(err), s_result.id,
                       s_result.take);
        return false;
    }
    RLOG_OPERATION("dispense start: id=%s take=%d pulses=%u", s_result.id, s_result.take,
                   (unsigned)s_plan.pulses);
    ui_show_dispensing(0, s_plan.pulses);  /* TODO：motor 每盤完成時回報進度 */
    return true;
}

static void show_menu(void)
{
    char ip[16] = "-";
    char info[MENU_INFO_MAX];
    const app_net_link_t link = active_link();
    active_ip(ip, sizeof ip);
    snprintf(info, sizeof info, "裝置 %s\n版本 %s\n網路 %s\nIP %s", s_settings.device_code,
             esp_app_get_description()->version,
             link == APP_NET_ETH ? "有線" : link == APP_NET_WIFI ? wifi_mgr_ssid() : "未連線", ip);
    ui_show_menu(info);
}

/* 進入新狀態時執行。需要立刻接著處理的事件寫到 follow，回傳 true。 */
static bool enter_state(app_state_t state, app_event_t *follow)
{
    switch (state) {
    case APP_ST_STANDBY:
        s_typed[0] = '\0';
        s_code[0] = '\0';
        scanner_reset_line();  /* 上一位沒打完的字不得接到下一筆碼前面 */
        ui_show_standby();
        return false;
    case APP_ST_INPUT:
        ui_show_input(s_typed, INPUT_TIMEOUT_MS);
        return false;
    case APP_ST_QUERY:
        ui_show_querying();
        *follow = do_query();
        return true;
    case APP_ST_CONFIRM:
        ui_show_confirm(s_result.take, s_result.point_before, s_result.point_after);
        *follow = do_confirm();
        return true;
    case APP_ST_DISPENSE:
        if (s_confirm_only) {
            /* 測試模式：點數已扣，沒有馬達可以跑。顯示後直接當作出球完成。 */
            ESP_LOGW(TAG, "TEST MODE: id=%s confirmed, dispense skipped", s_result.id);
            RLOG_OPERATION("test mode: id=%s take=%d confirmed, dispense skipped", s_result.id,
                           s_result.take);
            ui_show_notice("測試模式", "已扣點，未出球");
            vTaskDelay(pdMS_TO_TICKS(CONFIRM_ONLY_NOTICE_MS));
            *follow = APP_EV_MOTOR_DONE;
            return true;
        }
        if (!do_dispense()) {
            *follow = APP_EV_DISPENSE_FAIL;
            return true;
        }
        return false;
    case APP_ST_ERROR:
        ui_show_error(s_error);
        return false;
    case APP_ST_MENU:
        show_menu();
        return false;
    case APP_ST_UPDATE:
        ui_show_notice("系統更新中", "請勿關閉電源\n完成後會自動重新開機");
        return false;
    }
    return false;
}

/* ---- 事件迴圈 ---------------------------------------------------------- */

/* s_code 是維修碼時開系統選單，其他都當領球碼 */
static app_event_t line_event(void)
{
    return strcmp(s_code, MAINT_CODE) == 0 ? APP_EV_MAINT : APP_EV_LINE;
}

/* 記下輸入，回傳實際要處理的事件：觸控鍵盤按「確認」時，按鍵事件變成整行事件。 */
static app_event_t record_input(const app_msg_t *msg)
{
    const bool accepting = s_state == APP_ST_STANDBY || s_state == APP_ST_INPUT;
    if (!accepting) {
        return msg->event;
    }
    if (msg->event == APP_EV_KEY) {
        if (app_typed_apply(s_typed, sizeof s_typed, msg->text[0])) {
            strlcpy(s_code, s_typed, sizeof s_code);
            return line_event();
        }
    } else if (msg->event == APP_EV_LINE) {
        strlcpy(s_code, msg->text, sizeof s_code);
        return line_event();
    }
    return msg->event;
}

static void handle_event(app_event_t event, const app_msg_t *msg)
{
    for (;;) {
        if (msg != NULL) {
            event = record_input(msg);
            if (msg->event == APP_EV_MENU_CLOSE && s_state == APP_ST_MENU &&
                strcmp(msg->text, MENU_REBOOT_TEXT) == 0) {
                ESP_LOGW(TAG, "reboot from system menu");
                RLOG_SYSTEM("reboot from system menu");
                vTaskDelay(pdMS_TO_TICKS(REBOOT_FLUSH_MS));  /* 讓這一筆送出去 */
                esp_restart();
            }
            if (msg->event == APP_EV_MENU_CLOSE && s_state == APP_ST_MENU &&
                strcmp(msg->text, MENU_UPDATE_TEXT) == 0 && s_ota_task != NULL) {
                xTaskNotifyGive(s_ota_task);  /* 選單照常關閉，結果由更新工作顯示 */
            }
            if (msg->event == APP_EV_DISPENSE_FAIL && s_state == APP_ST_DISPENSE) {
                ESP_LOGE(TAG, "dispense aborted id=%s take=%d", s_result.id, s_result.take);
                RLOG_EXCEPTION("dispense aborted id=%s take=%d", s_result.id, s_result.take);
                set_error(msg->text);
            }
        }
        const app_state_t next = app_fsm_next(s_state, event);
        if (next == s_state) {
            if (s_state == APP_ST_INPUT && event == APP_EV_KEY) {
                ui_show_input(s_typed, INPUT_TIMEOUT_MS);
            }
            return;
        }
        ESP_LOGI(TAG, "%s -> %s", app_fsm_state_name(s_state), app_fsm_state_name(next));
        s_state = next;
        if (!enter_state(next, &event)) {
            return;
        }
        msg = NULL;  /* 接著處理 follow 事件 */
    }
}

static TickType_t state_timeout_ticks(app_state_t state)
{
    switch (state) {
    case APP_ST_INPUT: return pdMS_TO_TICKS(INPUT_TIMEOUT_MS);
    case APP_ST_ERROR: return pdMS_TO_TICKS(ERROR_SHOW_MS);
    case APP_ST_MENU:  return pdMS_TO_TICKS(MENU_TIMEOUT_MS);
    default:           return portMAX_DELAY;
    }
}

static app_event_t timeout_event(app_state_t state)
{
    return state == APP_ST_ERROR ? APP_EV_ERROR_SHOWN : APP_EV_TIMEOUT;
}

/*
 * 開機時從 TF 卡匯入現場設定與待機圖。沒插卡、沒有檔案都不算錯誤。
 * 必須在畫面、WiFi、API 啟動之前呼叫：匯入的內容這次開機就生效。
 * 回傳要顯示給安裝人員看的訊息（沒有匯入任何東西時是空字串）。
 */
static void import_from_card(char *notice, size_t notice_size)
{
    notice[0] = '\0';
    if (tfcard_mount() != ESP_OK) {
        return;
    }
    const int applied = settings_import_file(CONFIG_FILE_PATH, &s_settings);
    bool image_changed = false;
    const esp_err_t image_err = standby_store_import_file(STANDBY_FILE_PATH, &image_changed);
    tfcard_unmount();

    int len = 0;
    if (applied > 0) {
        len += snprintf(notice + len, notice_size - len, "已匯入 %d 項設定\n", applied);
    }
    if (image_changed) {
        len += snprintf(notice + len, notice_size - len, "待機圖已更新\n");
    } else if (image_err != ESP_OK && image_err != ESP_ERR_NOT_FOUND) {
        len += snprintf(notice + len, notice_size - len, "待機圖無法匯入\n");
    }
    if (len > 0) {
        snprintf(notice + len, notice_size - len, "可以取出記憶卡");
    }
}

static void init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void app_main(void)
{
    /* 馬達腳位最先初始化：越早固定在 LOW，越不會開機自動出球 */
    if (CONFIRM_WITHOUT_MOTOR) {
        ESP_LOGW(TAG, "TEST MODE ON: take_ball_confirm is sent without a motor (deducts points)");
    }
    if (motor_init(CONFIG_GOLF_MOTOR_GPIO) != ESP_OK) {
        ESP_LOGE(TAG, "motor disabled");
    }

    /* 台灣時間（UTC+8），給畫面時鐘用。API 簽章用的是 epoch 秒數，不受時區影響。 */
    setenv("TZ", "CST-8", 1);
    tzset();

    init_nvs();
    settings_load(&s_settings);
    s_queue = xQueueCreate(APP_QUEUE_LEN, sizeof(app_msg_t));
    configASSERT(s_queue != NULL);

    char card_notice[NOTICE_MAX];
    import_from_card(card_notice, sizeof card_notice);

    if (remote_log_start(s_settings.log_token, s_settings.device_code) != ESP_OK) {
        ESP_LOGE(TAG, "remote log disabled");
    }
    RLOG_SYSTEM("boot: version %s, reset reason %d%s", esp_app_get_description()->version,
                (int)esp_reset_reason(), ota_is_pending_verify() ? ", pending verify" : "");

    ESP_ERROR_CHECK(ui_init());
    ui_set_input_handlers(on_ui_touch, on_ui_key, NULL);
    ui_set_menu_handler(on_ui_menu, NULL);
    if (card_notice[0] != '\0') {
        ui_show_notice("記憶卡", card_notice);
        vTaskDelay(pdMS_TO_TICKS(NOTICE_SHOW_MS));
    }
    if (api_init(CONFIG_GOLF_API_BASE_URL, s_settings.device_code) != ESP_OK) {
        ESP_LOGE(TAG, "api disabled");
    }

    const wifi_mgr_ap_t aps[] = {
        {s_settings.wifi1_ssid, s_settings.wifi1_password},
        {s_settings.wifi2_ssid, s_settings.wifi2_password},
    };
    if (wifi_mgr_start(aps, sizeof aps / sizeof aps[0], on_wifi_status, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "wifi disabled");
    }
    /* 有線網路：有接線就優先使用，沒接線不影響 WiFi */
    if (eth_mgr_start(on_eth_status, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "ethernet disabled");
    }
    if (scanner_start(on_scanner_line, on_scanner_key, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "scanner disabled");
    }
    /* 鏡頭掃 QR Code。沒有鏡頭或啟動失敗時，鍵盤與掃碼器照常可用。 */
    const camera_observer_t observer = {
        .on_status = on_camera_status, .on_preview = on_camera_preview};
    camera_set_observer(&observer);
    if (camera_start(on_camera_code, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "camera disabled");
    }
    ui_set_debug_key_handler(on_debug_key);

    if (xTaskCreate(ping_task, "ping", PING_TASK_STACK, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "status report disabled");
    }

    if (s_settings.ota_url[0] == '\0') {
        ESP_LOGI(TAG, "ota off (no ota_url)");
    } else if (xTaskCreate(ota_task, "ota", OTA_TASK_STACK, NULL, 2, &s_ota_task) != pdPASS) {
        ESP_LOGE(TAG, "ota disabled");
    }

    app_event_t unused;
    enter_state(APP_ST_STANDBY, &unused);

    for (;;) {
        app_msg_t msg;
        if (xQueueReceive(s_queue, &msg, state_timeout_ticks(s_state)) == pdTRUE) {
            handle_event(msg.event, &msg);
        } else {
            handle_event(timeout_event(s_state), NULL);
        }
    }
}
