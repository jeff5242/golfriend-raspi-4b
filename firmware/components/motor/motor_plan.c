#include "motor_plan.h"

#include <math.h>
#include <stddef.h>

#define BALLS_PER_TIMED_STEP 5.0f      /* BallControl.getReverseTime：每 5 顆一個 timer */
#define MOTOR_PLAN_MAX_COUNT 4.0e9f    /* 毫秒數、盤數必須放得進 uint32_t */

/* 伺服器的值經 strtod 轉來，可能是 inf、nan 或大到轉 uint32_t 會出錯的數 */
static bool to_u32(float value, uint32_t *out)
{
    if (!isfinite(value) || value < 0.0f || value > MOTOR_PLAN_MAX_COUNT) {
        return false;
    }
    *out = (uint32_t)value;
    return true;
}

static bool seconds_to_ms(float seconds, uint32_t *out_ms)
{
    return to_u32(roundf(seconds * 1000.0f), out_ms);
}

static motor_plan_err_t build_timed(const motor_params_t *params, motor_plan_t *plan)
{
    if (!(params->timer_s > 0.0f)) {  /* 寫成 !(x > 0) 才擋得住 nan */
        return MOTOR_PLAN_ERR_ARG;
    }
    const float steps = ceilf((float)params->take / BALLS_PER_TIMED_STEP);
    *plan = (motor_plan_t){.mode = MOTOR_MODE_TIMED, .pulses = 1, .off_ms = 0};
    return seconds_to_ms(steps * params->timer_s, &plan->on_ms) ? MOTOR_PLAN_OK
                                                                : MOTOR_PLAN_ERR_ARG;
}

static motor_plan_err_t build_plate(const motor_params_t *params, motor_plan_t *plan)
{
    if (!(params->ball_per_plate > 0.0f) || !(params->start_timer_s > 0.0f) ||
        !(params->interval_timer_s >= 0.0f)) {
        return MOTOR_PLAN_ERR_ARG;
    }
    *plan = (motor_plan_t){.mode = MOTOR_MODE_PLATE};
    const bool ok =
        to_u32(ceilf((float)params->take / params->ball_per_plate), &plan->pulses) &&
        seconds_to_ms(params->start_timer_s, &plan->on_ms) &&
        seconds_to_ms(params->interval_timer_s, &plan->off_ms);
    return ok ? MOTOR_PLAN_OK : MOTOR_PLAN_ERR_ARG;
}

motor_plan_err_t motor_plan_build(const motor_params_t *params, motor_plan_t *out)
{
    if (params == NULL || out == NULL || params->take <= 0) {
        return MOTOR_PLAN_ERR_ARG;
    }

    motor_plan_t plan;
    motor_plan_err_t err;
    if (params->type == MOTOR_MODE_TIMED) {
        err = build_timed(params, &plan);
    } else if (params->type == MOTOR_MODE_PLATE) {
        err = build_plate(params, &plan);
    } else {
        return MOTOR_PLAN_ERR_UNSUPPORTED;
    }
    if (err != MOTOR_PLAN_OK) {
        return err;
    }
    /* 四捨五入成 0 ms 或 0 盤的時序 motor_run 會拒絕；必須在送確認（扣點）前就擋下 */
    if (!motor_plan_is_runnable(&plan)) {
        return MOTOR_PLAN_ERR_ARG;
    }
    *out = plan;
    return MOTOR_PLAN_OK;
}

bool motor_plan_is_runnable(const motor_plan_t *plan)
{
    return plan != NULL && plan->pulses > 0 && plan->on_ms > 0;
}
