#include "motor_plan.h"
#include "test_util.h"

static void test_timed_mode_matches_ballcontrol(void)
{
    /* BallControl.getReverseTime：ceil(12 / 5) = 3，3 * 0.75 秒 = 2250 ms */
    motor_params_t p = {.type = 1, .take = 12, .timer_s = 0.75f};
    motor_plan_t plan;
    CHECK(motor_plan_build(&p, &plan) == MOTOR_PLAN_OK);
    CHECK(plan.mode == MOTOR_MODE_TIMED);
    CHECK(plan.pulses == 1);
    CHECK(plan.on_ms == 2250);
    CHECK(plan.off_ms == 0);
}

static void test_plate_mode_tiezhenshan_settings(void)
{
    /* 鐵砧山 2026-10-25 後台：一盤 30 顆、出盤啟動 1.0 秒、間隔 6.0 秒；取 60 顆 = 2 盤 */
    motor_params_t p = {.type = 2, .take = 60, .start_timer_s = 1.0f,
                        .interval_timer_s = 6.0f, .ball_per_plate = 30.0f};
    motor_plan_t plan;
    CHECK(motor_plan_build(&p, &plan) == MOTOR_PLAN_OK);
    CHECK(plan.mode == MOTOR_MODE_PLATE);
    CHECK(plan.pulses == 2);
    CHECK(plan.on_ms == 1000);
    CHECK(plan.off_ms == 6000);
}

static void test_plate_mode_rounds_up_partial_plate(void)
{
    motor_params_t p = {.type = 2, .take = 31, .start_timer_s = 0.12f,
                        .interval_timer_s = 6.0f, .ball_per_plate = 30.0f};
    motor_plan_t plan;
    CHECK(motor_plan_build(&p, &plan) == MOTOR_PLAN_OK);
    CHECK(plan.pulses == 2);
    CHECK(plan.on_ms == 120);
}

static void test_invalid_params_rejected(void)
{
    motor_plan_t plan;
    motor_params_t zero_take = {.type = 1, .take = 0, .timer_s = 0.75f};
    motor_params_t zero_timer = {.type = 1, .take = 5, .timer_s = 0.0f};
    motor_params_t zero_per_plate = {.type = 2, .take = 30, .start_timer_s = 1.0f,
                                     .interval_timer_s = 6.0f, .ball_per_plate = 0.0f};
    motor_params_t zero_start = {.type = 2, .take = 30, .start_timer_s = 0.0f,
                                 .interval_timer_s = 6.0f, .ball_per_plate = 30.0f};
    CHECK(motor_plan_build(NULL, &plan) == MOTOR_PLAN_ERR_ARG);
    CHECK(motor_plan_build(&zero_take, NULL) == MOTOR_PLAN_ERR_ARG);
    CHECK(motor_plan_build(&zero_take, &plan) == MOTOR_PLAN_ERR_ARG);
    CHECK(motor_plan_build(&zero_timer, &plan) == MOTOR_PLAN_ERR_ARG);
    CHECK(motor_plan_build(&zero_per_plate, &plan) == MOTOR_PLAN_ERR_ARG);
    CHECK(motor_plan_build(&zero_start, &plan) == MOTOR_PLAN_ERR_ARG);
}

static void test_sensor_mode_unsupported(void)
{
    motor_params_t p = {.type = 0, .take = 30, .timer_s = 0.75f};
    motor_plan_t plan;
    CHECK(motor_plan_build(&p, &plan) == MOTOR_PLAN_ERR_UNSUPPORTED);
}

int main(void)
{
    RUN(test_timed_mode_matches_ballcontrol);
    RUN(test_plate_mode_tiezhenshan_settings);
    RUN(test_plate_mode_rounds_up_partial_plate);
    RUN(test_invalid_params_rejected);
    RUN(test_sensor_mode_unsupported);
    return TEST_SUMMARY();
}
