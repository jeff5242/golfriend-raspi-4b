#include <math.h>

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

static void test_rounds_to_zero_rejected(void)
{
    /* 審查 P2：伺服器送極小秒數，通電毫秒四捨五入成 0，motor_run 會拒絕，客人被扣點卻沒球 */
    motor_plan_t plan;
    motor_params_t tiny_timer = {.type = 1, .take = 1, .timer_s = 0.0001f};
    motor_params_t tiny_start = {.type = 2, .take = 30, .start_timer_s = 0.0004f,
                                 .interval_timer_s = 6.0f, .ball_per_plate = 30.0f};
    CHECK(motor_plan_build(&tiny_timer, &plan) == MOTOR_PLAN_ERR_ARG);
    CHECK(motor_plan_build(&tiny_start, &plan) == MOTOR_PLAN_ERR_ARG);
}

static void test_non_finite_and_huge_rejected(void)
{
    /* strtod 會把 "inf"、"nan" 轉成 INFINITY、NAN；過大的值轉成 uint32 是未定義行為 */
    motor_plan_t plan;
    motor_params_t inf_per_plate = {.type = 2, .take = 60, .start_timer_s = 1.0f,
                                    .interval_timer_s = 6.0f, .ball_per_plate = INFINITY};
    motor_params_t nan_timer = {.type = 1, .take = 5, .timer_s = NAN};
    motor_params_t inf_timer = {.type = 1, .take = 5, .timer_s = INFINITY};
    motor_params_t nan_interval = {.type = 2, .take = 30, .start_timer_s = 1.0f,
                                   .interval_timer_s = NAN, .ball_per_plate = 30.0f};
    motor_params_t huge_timer = {.type = 1, .take = 5, .timer_s = 1.0e9f};
    motor_params_t tiny_per_plate = {.type = 2, .take = 60, .start_timer_s = 1.0f,
                                     .interval_timer_s = 6.0f, .ball_per_plate = 1.0e-30f};
    CHECK(motor_plan_build(&inf_per_plate, &plan) == MOTOR_PLAN_ERR_ARG);
    CHECK(motor_plan_build(&nan_timer, &plan) == MOTOR_PLAN_ERR_ARG);
    CHECK(motor_plan_build(&inf_timer, &plan) == MOTOR_PLAN_ERR_ARG);
    CHECK(motor_plan_build(&nan_interval, &plan) == MOTOR_PLAN_ERR_ARG);
    CHECK(motor_plan_build(&huge_timer, &plan) == MOTOR_PLAN_ERR_ARG);
    CHECK(motor_plan_build(&tiny_per_plate, &plan) == MOTOR_PLAN_ERR_ARG);
}

static void test_is_runnable_matches_motor_run_rules(void)
{
    const motor_plan_t ok = {.mode = MOTOR_MODE_TIMED, .pulses = 1, .on_ms = 1, .off_ms = 0};
    const motor_plan_t no_pulse = {.mode = MOTOR_MODE_PLATE, .pulses = 0, .on_ms = 100};
    const motor_plan_t no_on = {.mode = MOTOR_MODE_TIMED, .pulses = 1, .on_ms = 0};
    CHECK(motor_plan_is_runnable(&ok));
    CHECK(!motor_plan_is_runnable(&no_pulse));
    CHECK(!motor_plan_is_runnable(&no_on));
    CHECK(!motor_plan_is_runnable(NULL));
}

int main(void)
{
    RUN(test_timed_mode_matches_ballcontrol);
    RUN(test_plate_mode_tiezhenshan_settings);
    RUN(test_plate_mode_rounds_up_partial_plate);
    RUN(test_invalid_params_rejected);
    RUN(test_sensor_mode_unsupported);
    RUN(test_rounds_to_zero_rejected);
    RUN(test_non_finite_and_huge_rejected);
    RUN(test_is_runnable_matches_motor_run_rules);
    return TEST_SUMMARY();
}
