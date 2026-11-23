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
    CHECK(app_dispense_prepare(&p, false, false, &plan) == APP_DISPENSE_MOTOR_NOT_READY);
}

static void test_unsupported_type_blocks_before_confirm(void)
{
    /* 審查 P1：後台回原型不支援的 type（光感模式 0），必須在送確認之前擋下 */
    motor_params_t p = timed_params();
    p.type = 0;
    motor_plan_t plan;
    CHECK(app_dispense_prepare(&p, true, false, &plan) == APP_DISPENSE_BAD_PLAN);
}

static void test_bad_values_block_before_confirm(void)
{
    motor_params_t p = timed_params();
    p.take = 0;
    motor_plan_t plan;
    CHECK(app_dispense_prepare(&p, true, false, &plan) == APP_DISPENSE_BAD_PLAN);
    CHECK(app_dispense_prepare(NULL, true, false, &plan) == APP_DISPENSE_BAD_PLAN);
}

static void test_ready_and_valid_returns_plan(void)
{
    const motor_params_t p = timed_params();
    motor_plan_t plan = {0};
    CHECK(app_dispense_prepare(&p, true, false, &plan) == APP_DISPENSE_OK);
    CHECK(plan.mode == MOTOR_MODE_TIMED);
    CHECK(plan.pulses == 1);
    CHECK(plan.on_ms == 6000);
}

static void test_confirm_test_mode_skips_motor_only_when_motor_missing(void)
{
    /* 測試用開關：沒接馬達時也送確認（會扣點），但不出球 */
    const motor_params_t p = timed_params();
    motor_plan_t plan = {0};
    CHECK(app_dispense_prepare(&p, false, true, &plan) == APP_DISPENSE_CONFIRM_ONLY);
    /* 後台參數算不出時序也照樣送確認：這個模式只驗證扣點 */
    CHECK(app_dispense_prepare(NULL, false, true, &plan) == APP_DISPENSE_CONFIRM_ONLY);
}

static void test_confirm_test_mode_is_ignored_when_motor_ready(void)
{
    /* 馬達接上後，開關不得改變正常流程 */
    motor_params_t p = timed_params();
    motor_plan_t plan = {0};
    CHECK(app_dispense_prepare(&p, true, true, &plan) == APP_DISPENSE_OK);
    CHECK(plan.on_ms == 6000);
    p.take = 0;
    CHECK(app_dispense_prepare(&p, true, true, &plan) == APP_DISPENSE_BAD_PLAN);
}

int main(void)
{
    RUN(test_motor_not_ready_blocks_before_confirm);
    RUN(test_unsupported_type_blocks_before_confirm);
    RUN(test_bad_values_block_before_confirm);
    RUN(test_ready_and_valid_returns_plan);
    RUN(test_confirm_test_mode_skips_motor_only_when_motor_missing);
    RUN(test_confirm_test_mode_is_ignored_when_motor_ready);
    return TEST_SUMMARY();
}
