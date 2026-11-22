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
#include <string.h>

#include "api.h"
#include "api_errors.h"
#include "app_dispense.h"
#include "app_fsm.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "motor.h"
#include "motor_plan.h"
#include "nvs_flash.h"
#include "scanner.h"
#include "sdkconfig.h"
#include "ui.h"
#include "wifi_mgr.h"

static const char *TAG = "app";

#define INPUT_TIMEOUT_MS 10000  /* 同 Pi 版 TakeBallDialog 10 秒 */
#define CONFIRM_DELAY_MS 2000   /* 同 Pi 版 QR 路徑 2 秒自動確認 */
#define ERROR_SHOW_MS    3000
#define APP_QUEUE_LEN    16
#define ERROR_TEXT_MAX   64
#define API_CODE_MAX     16

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

static void on_scanner_key(char c, void *ctx)
{
    (void)ctx;
    const char s[2] = {c, '\0'};
    post(APP_EV_KEY, s);
}

static void on_motor_done(esp_err_t result, void *ctx)
{
    (void)ctx;
    if (result != ESP_OK) {
        post(APP_EV_DISPENSE_FAIL, "出球中斷，請洽櫃台");  /* 交易已確認，球沒出完 */
        return;
    }
    post(APP_EV_MOTOR_DONE, NULL);
}

static void on_wifi_status(bool connected, int ap_index, void *ctx)
{
    (void)ctx;
    static bool sntp_started = false;
    ESP_LOGI(TAG, "wifi %s (ap %d)", connected ? "up" : "down", ap_index);
    if (connected && !sntp_started) {
        sntp_started = api_time_sync_start() == ESP_OK;  /* 失敗時下次連上再試 */
    }
    ui_set_wifi_status(connected, wifi_mgr_rssi());
}

/* ---- 各狀態要做的事 ---------------------------------------------------- */

static void set_error(const char *text)
{
    strlcpy(s_error, text, sizeof s_error);
}

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
    const app_dispense_check_t check = app_dispense_prepare(&params, motor_is_ready(), &s_plan);
    if (check == APP_DISPENSE_OK) {
        return true;
    }
    ESP_LOGE(TAG, "not confirming id=%s: check=%d type=%d take=%d", s_result.id, (int)check,
             s_result.type, s_result.take);
    set_error(check == APP_DISPENSE_MOTOR_NOT_READY ? "出球機未就緒，請洽櫃台"
                                                    : "出球設定錯誤，請洽櫃台");
    return false;
}

static app_event_t do_query(void)
{
    if (!wifi_mgr_is_connected()) {
        set_error("網路連接錯誤");
        return APP_EV_API_FAIL;
    }
    char code[API_CODE_MAX];
    const esp_err_t err = api_take_ball(s_code, &s_result, code, sizeof code);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "take_ball ok: id=%s take=%d type=%d", s_result.id, s_result.take,
                 s_result.type);
        return prepare_dispense() ? APP_EV_API_OK : APP_EV_API_FAIL;
    }
    /* 螢幕還沒接上時，只能從 log 看伺服器回了什麼錯誤代碼 */
    ESP_LOGW(TAG, "take_ball failed: %s code=%s", esp_err_to_name(err), code);
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
        return APP_EV_CONFIRM_OK;
    }
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
        return false;
    }
    ui_show_dispensing(0, s_plan.pulses);  /* TODO：motor 每盤完成時回報進度 */
    return true;
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
        ui_show_input(s_typed);
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
        if (!do_dispense()) {
            *follow = APP_EV_DISPENSE_FAIL;
            return true;
        }
        return false;
    case APP_ST_ERROR:
        ui_show_error(s_error);
        return false;
    }
    return false;
}

/* ---- 事件迴圈 ---------------------------------------------------------- */

static void record_input(const app_msg_t *msg)
{
    const bool accepting = s_state == APP_ST_STANDBY || s_state == APP_ST_INPUT;
    if (!accepting) {
        return;
    }
    if (msg->event == APP_EV_KEY) {
        strlcat(s_typed, msg->text, sizeof s_typed);
    } else if (msg->event == APP_EV_LINE) {
        /* TODO：維修碼（Pi 版 7777777 關機）在 MCU 上改成什麼動作，待決定 */
        strlcpy(s_code, msg->text, sizeof s_code);
    }
}

static void handle_event(app_event_t event, const app_msg_t *msg)
{
    for (;;) {
        if (msg != NULL) {
            record_input(msg);
            if (msg->event == APP_EV_DISPENSE_FAIL && s_state == APP_ST_DISPENSE) {
                ESP_LOGE(TAG, "dispense aborted id=%s take=%d", s_result.id, s_result.take);
                set_error(msg->text);
            }
        }
        const app_state_t next = app_fsm_next(s_state, event);
        if (next == s_state) {
            if (s_state == APP_ST_INPUT && event == APP_EV_KEY) {
                ui_show_input(s_typed);
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
    default:           return portMAX_DELAY;
    }
}

static app_event_t timeout_event(app_state_t state)
{
    return state == APP_ST_ERROR ? APP_EV_ERROR_SHOWN : APP_EV_TIMEOUT;
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
    if (motor_init(CONFIG_GOLF_MOTOR_GPIO) != ESP_OK) {
        ESP_LOGE(TAG, "motor disabled");
    }

    init_nvs();
    s_queue = xQueueCreate(APP_QUEUE_LEN, sizeof(app_msg_t));
    configASSERT(s_queue != NULL);

    ESP_ERROR_CHECK(ui_init());
    if (api_init(CONFIG_GOLF_API_BASE_URL, CONFIG_GOLF_DEVICE_CODE) != ESP_OK) {
        ESP_LOGE(TAG, "api disabled");
    }

    const wifi_mgr_ap_t aps[] = {
        {CONFIG_GOLF_WIFI1_SSID, CONFIG_GOLF_WIFI1_PASSWORD},
        {CONFIG_GOLF_WIFI2_SSID, CONFIG_GOLF_WIFI2_PASSWORD},
    };
    if (wifi_mgr_start(aps, sizeof aps / sizeof aps[0], on_wifi_status, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "wifi disabled");
    }
    if (scanner_start(on_scanner_line, on_scanner_key, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "scanner disabled");
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
