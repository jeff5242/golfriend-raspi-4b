#pragma once
/*
 * 由 take_ball API 的回應算出馬達的通電時序。
 * 純邏輯，不依賴 ESP-IDF，可以在主機端測試。
 *
 * 對應 Pi 版：
 *   type 1（計時）：BallControl.getReverseTime() = ceil(take / 5) * timer，馬達通電一次
 *   type 2（計盤）：MediaPlayer.SuccessDialog
 *                   盤數 = ceil(take / ball_per_plate)
 *                   每盤通電 start_timer 秒，盤與盤之間斷電 interval_timer 秒
 *   其他 type（光感模式）原型不支援。
 */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MOTOR_MODE_TIMED = 1,
    MOTOR_MODE_PLATE = 2,
} motor_mode_t;

typedef struct {
    int type;                /* API 的 type */
    int take;                /* 取球數 */
    float timer_s;           /* type 1 用：每 5 顆的秒數 */
    float start_timer_s;     /* type 2 用：每盤通電秒數 */
    float interval_timer_s;  /* type 2 用：盤與盤之間的斷電秒數 */
    float ball_per_plate;    /* type 2 用：一盤幾顆 */
} motor_params_t;

typedef struct {
    motor_mode_t mode;
    uint32_t pulses;         /* 通電次數（計時模式固定 1） */
    uint32_t on_ms;          /* 每次通電毫秒 */
    uint32_t off_ms;         /* 兩次通電之間的斷電毫秒（計時模式為 0） */
} motor_plan_t;

typedef enum {
    MOTOR_PLAN_OK = 0,
    MOTOR_PLAN_ERR_ARG = -1,          /* 參數為 NULL、取球數或秒數不合理 */
    MOTOR_PLAN_ERR_UNSUPPORTED = -2,  /* 原型不支援的 type（光感模式） */
} motor_plan_err_t;

motor_plan_err_t motor_plan_build(const motor_params_t *params, motor_plan_t *out);

#ifdef __cplusplus
}
#endif
