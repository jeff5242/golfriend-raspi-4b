#include "app_dispense.h"

#include <stddef.h>

app_dispense_check_t app_dispense_prepare(const motor_params_t *params, bool motor_ready,
                                          motor_plan_t *plan)
{
    if (!motor_ready) {
        return APP_DISPENSE_MOTOR_NOT_READY;
    }
    if (motor_plan_build(params, plan) != MOTOR_PLAN_OK) {
        return APP_DISPENSE_BAD_PLAN;
    }
    return APP_DISPENSE_OK;
}
