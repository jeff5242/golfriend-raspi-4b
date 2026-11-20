#include "app_dispense.h"
#include "test_util.h"

static motor_params_t timed_params(void)
{
    return (motor_params_t){.type = MOTOR_MODE_TIMED, .take = 10, .timer_s = 3.0f};
}

static void test_motor_not_ready_blocks_before_confirm(void)
{
    /* 審查 P1：馬達腳位沒設定時，必須在送確認（扣點）之前擋下 */
    const motor_params_t p = timed_params();
    motor_plan_t plan;
    CHECK(app_dispense_prepare(&p, false, &plan) == APP_DISPENSE_MOTOR_NOT_READY);
}

static void test_unsupported_type_blocks_before_confirm(void)
{
    /* 審查 P1：後台回原型不支援的 type（光感模式 0），必須在送確認之前擋下 */
    motor_params_t p = timed_params();
    p.type = 0;
    motor_plan_t plan;
    CHECK(app_dispense_prepare(&p, true, &plan) == APP_DISPENSE_BAD_PLAN);
}

static void test_bad_values_block_before_confirm(void)
{
    motor_params_t p = timed_params();
    p.take = 0;
    motor_plan_t plan;
    CHECK(app_dispense_prepare(&p, true, &plan) == APP_DISPENSE_BAD_PLAN);
    CHECK(app_dispense_prepare(NULL, true, &plan) == APP_DISPENSE_BAD_PLAN);
}

static void test_ready_and_valid_returns_plan(void)
{
    const motor_params_t p = timed_params();
    motor_plan_t plan = {0};
    CHECK(app_dispense_prepare(&p, true, &plan) == APP_DISPENSE_OK);
    CHECK(plan.mode == MOTOR_MODE_TIMED);
    CHECK(plan.pulses == 1);
    CHECK(plan.on_ms == 6000);
}

int main(void)
{
    RUN(test_motor_not_ready_blocks_before_confirm);
    RUN(test_unsupported_type_blocks_before_confirm);
    RUN(test_bad_values_block_before_confirm);
    RUN(test_ready_and_valid_returns_plan);
    return TEST_SUMMARY();
}
