#pragma once
/*
 * 送 take_ball_confirm（扣點）之前，先確認這一筆真的出得了球。
 * 純邏輯，可以在主機端測試。
 *
 * Pi 版是先確認再出球；MCU 版改成先檢查，避免扣了點卻不出球。
 */
#include <stdbool.h>

#include "motor_plan.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_DISPENSE_OK = 0,
    APP_DISPENSE_MOTOR_NOT_READY,  /* 馬達腳位沒設定、初始化失敗或正在跑 */
    APP_DISPENSE_BAD_PLAN,         /* 後台回的 type 或參數算不出時序 */
    APP_DISPENSE_CONFIRM_ONLY,     /* 測試用：沒接馬達，只送確認（會扣點）、不出球 */
} app_dispense_check_t;

/*
 * 回傳 APP_DISPENSE_OK 時，時序寫到 plan。
 * confirm_without_motor 是測試用開關：只在馬達未就緒時生效，回傳 APP_DISPENSE_CONFIRM_ONLY。
 */
app_dispense_check_t app_dispense_prepare(const motor_params_t *params, bool motor_ready,
                                          bool confirm_without_motor, motor_plan_t *plan);

#ifdef __cplusplus
}
#endif
