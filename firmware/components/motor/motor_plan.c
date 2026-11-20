#include "motor_plan.h"

#include <math.h>
#include <stddef.h>

#define BALLS_PER_TIMED_STEP 5.0f  /* BallControl.getReverseTime：每 5 顆一個 timer */

static uint32_t seconds_to_ms(float seconds)
{
    return (uint32_t)lroundf(seconds * 1000.0f);
}

motor_plan_err_t motor_plan_build(const motor_params_t *params, motor_plan_t *out)
{
    if (params == NULL || out == NULL || params->take <= 0) {
        return MOTOR_PLAN_ERR_ARG;
    }

    if (params->type == MOTOR_MODE_TIMED) {
        if (params->timer_s <= 0.0f) {
            return MOTOR_PLAN_ERR_ARG;
        }
        const float steps = ceilf((float)params->take / BALLS_PER_TIMED_STEP);
        *out = (motor_plan_t){
            .mode = MOTOR_MODE_TIMED,
            .pulses = 1,
            .on_ms = seconds_to_ms(steps * params->timer_s),
            .off_ms = 0,
        };
        return MOTOR_PLAN_OK;
    }

    if (params->type == MOTOR_MODE_PLATE) {
        if (params->ball_per_plate <= 0.0f || params->start_timer_s <= 0.0f ||
            params->interval_timer_s < 0.0f) {
            return MOTOR_PLAN_ERR_ARG;
        }
        *out = (motor_plan_t){
            .mode = MOTOR_MODE_PLATE,
            .pulses = (uint32_t)ceilf((float)params->take / params->ball_per_plate),
            .on_ms = seconds_to_ms(params->start_timer_s),
            .off_ms = seconds_to_ms(params->interval_timer_s),
        };
        return MOTOR_PLAN_OK;
    }

    return MOTOR_PLAN_ERR_UNSUPPORTED;
}
