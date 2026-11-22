#pragma once
/*
 * 後台 API 客戶端（對應 Pi 版 HttpCmdLibrary.py）。
 * 端點：<base_url>/take_ball、<base_url>/take_ball_confirm
 * 呼叫前必須先完成 SNTP 對時，簽章用的時間戳記才會對。
 */
#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define API_POINT_HIDDEN 999999  /* 後台回 999999 時畫面顯示 "------" */

typedef struct {
    char id[48];             /* body.id，確認時當 transaction_code */
    int take;                /* body.take，取球數 */
    int type;                /* body.type，1 計時、2 計盤 */
    float timer_s;           /* body.timer */
    float start_timer_s;     /* body.start_timer */
    float interval_timer_s;  /* body.interval_timer */
    float ball_per_plate;    /* body.ball_per_plate */
    long point_before;       /* body.point1 */
    long point_after;        /* body.point2 */
} api_take_ball_result_t;

/* base_url 例："http://golfpoint.milkidea.com/api"；device_code 對應 Pi 的 Desktop/DeviceName */
esp_err_t api_init(const char *base_url, const char *device_code);

/* 啟動 SNTP 對時。WiFi 拿到 IP 後呼叫；回傳 ESP_OK 前，下次連上必須再呼叫。 */
esp_err_t api_time_sync_start(void);

/* 系統時間已經對過（年份 >= 2024）時回傳 true。 */
bool api_time_is_valid(void);

/*
 * 送 take_ball。
 * ESP_OK：後台回 error=false，結果寫到 out。
 * ESP_FAIL：後台回 error=true，錯誤代碼字串寫到 err_msg。
 * ESP_ERR_INVALID_STATE：還沒對時。其他：網路錯誤。
 */
esp_err_t api_take_ball(const char *transaction_code, api_take_ball_result_t *out,
                        char *err_msg, size_t err_size);

/* 送 take_ball_confirm。回傳值規則同 api_take_ball。 */
esp_err_t api_take_ball_confirm(const char *response_id, const char *note, const char *status,
                                char *err_msg, size_t err_size);

/*
 * 送 take_ball_ping（定期回報）。note 是「IP_版本」，同 Pi 版的 tmpIp + '_' + appVersion。
 * 後台用這個請求更新裝置時間與上線狀態。回傳值規則同 api_take_ball。
 */
esp_err_t api_ping(const char *note, char *err_msg, size_t err_size);

#ifdef __cplusplus
}
#endif
