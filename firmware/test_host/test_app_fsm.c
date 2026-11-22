#include "app_fsm.h"
#include "test_util.h"

static void test_happy_path_manual_input(void)
{
    app_state_t s = APP_ST_STANDBY;
    s = app_fsm_next(s, APP_EV_KEY);
    CHECK(s == APP_ST_INPUT);
    s = app_fsm_next(s, APP_EV_KEY);
    CHECK(s == APP_ST_INPUT);
    s = app_fsm_next(s, APP_EV_LINE);
    CHECK(s == APP_ST_QUERY);
    s = app_fsm_next(s, APP_EV_API_OK);
    CHECK(s == APP_ST_CONFIRM);
    s = app_fsm_next(s, APP_EV_CONFIRM_OK);
    CHECK(s == APP_ST_DISPENSE);
    s = app_fsm_next(s, APP_EV_MOTOR_DONE);
    CHECK(s == APP_ST_STANDBY);
}

static void test_scanner_line_from_standby_goes_to_query(void)
{
    CHECK(app_fsm_next(APP_ST_STANDBY, APP_EV_LINE) == APP_ST_QUERY);
}

static void test_input_timeout_returns_to_standby(void)
{
    CHECK(app_fsm_next(APP_ST_INPUT, APP_EV_TIMEOUT) == APP_ST_STANDBY);
}

static void test_failures_go_to_error_then_standby(void)
{
    CHECK(app_fsm_next(APP_ST_QUERY, APP_EV_API_FAIL) == APP_ST_ERROR);
    CHECK(app_fsm_next(APP_ST_QUERY, APP_EV_TIMEOUT) == APP_ST_ERROR);
    CHECK(app_fsm_next(APP_ST_CONFIRM, APP_EV_API_FAIL) == APP_ST_ERROR);
    CHECK(app_fsm_next(APP_ST_ERROR, APP_EV_ERROR_SHOWN) == APP_ST_STANDBY);
}

static void test_dispense_failure_goes_to_error(void)
{
    /* 審查 P1：出球失敗必須進錯誤畫面停 3 秒，不得直接回待機蓋掉錯誤訊息 */
    CHECK(app_fsm_next(APP_ST_DISPENSE, APP_EV_DISPENSE_FAIL) == APP_ST_ERROR);
    CHECK(app_fsm_next(APP_ST_DISPENSE, APP_EV_MOTOR_DONE) == APP_ST_STANDBY);
    CHECK(app_fsm_next(APP_ST_STANDBY, APP_EV_DISPENSE_FAIL) == APP_ST_STANDBY);
}

static void test_input_ignored_while_busy(void)
{
    /* 查詢、確認、出球中收到掃碼，不得開始第二筆交易 */
    CHECK(app_fsm_next(APP_ST_QUERY, APP_EV_LINE) == APP_ST_QUERY);
    CHECK(app_fsm_next(APP_ST_CONFIRM, APP_EV_KEY) == APP_ST_CONFIRM);
    CHECK(app_fsm_next(APP_ST_DISPENSE, APP_EV_LINE) == APP_ST_DISPENSE);
    CHECK(app_fsm_next(APP_ST_DISPENSE, APP_EV_TIMEOUT) == APP_ST_DISPENSE);
}

static void test_state_names(void)
{
    CHECK_STR(app_fsm_state_name(APP_ST_STANDBY), "STANDBY");
    CHECK_STR(app_fsm_state_name(APP_ST_DISPENSE), "DISPENSE");
    CHECK_STR(app_fsm_state_name((app_state_t)99), "UNKNOWN");
}

static void test_touch_opens_input_only_from_standby(void)
{
    /* 點待機畫面：開啟觸控鍵盤（輸入狀態）。其他狀態點螢幕不得打斷流程 */
    CHECK(app_fsm_next(APP_ST_STANDBY, APP_EV_TOUCH) == APP_ST_INPUT);
    CHECK(app_fsm_next(APP_ST_INPUT, APP_EV_TOUCH) == APP_ST_INPUT);
    CHECK(app_fsm_next(APP_ST_QUERY, APP_EV_TOUCH) == APP_ST_QUERY);
    CHECK(app_fsm_next(APP_ST_CONFIRM, APP_EV_TOUCH) == APP_ST_CONFIRM);
    CHECK(app_fsm_next(APP_ST_DISPENSE, APP_EV_TOUCH) == APP_ST_DISPENSE);
    CHECK(app_fsm_next(APP_ST_ERROR, APP_EV_TOUCH) == APP_ST_ERROR);
}

static void test_maintenance_code_opens_menu(void)
{
    /* 維修碼只在待機與輸入時有效；選單關閉或逾時回待機 */
    CHECK(app_fsm_next(APP_ST_STANDBY, APP_EV_MAINT) == APP_ST_MENU);
    CHECK(app_fsm_next(APP_ST_INPUT, APP_EV_MAINT) == APP_ST_MENU);
    CHECK(app_fsm_next(APP_ST_QUERY, APP_EV_MAINT) == APP_ST_QUERY);
    CHECK(app_fsm_next(APP_ST_DISPENSE, APP_EV_MAINT) == APP_ST_DISPENSE);
    CHECK(app_fsm_next(APP_ST_MENU, APP_EV_MENU_CLOSE) == APP_ST_STANDBY);
    CHECK(app_fsm_next(APP_ST_MENU, APP_EV_TIMEOUT) == APP_ST_STANDBY);
    CHECK(app_fsm_next(APP_ST_MENU, APP_EV_LINE) == APP_ST_MENU);  /* 選單中掃碼不領球 */
    CHECK(app_fsm_next(APP_ST_MENU, APP_EV_KEY) == APP_ST_MENU);
    CHECK_STR(app_fsm_state_name(APP_ST_MENU), "MENU");
}

int main(void)
{
    RUN(test_happy_path_manual_input);
    RUN(test_scanner_line_from_standby_goes_to_query);
    RUN(test_input_timeout_returns_to_standby);
    RUN(test_failures_go_to_error_then_standby);
    RUN(test_dispense_failure_goes_to_error);
    RUN(test_input_ignored_while_busy);
    RUN(test_touch_opens_input_only_from_standby);
    RUN(test_maintenance_code_opens_menu);
    RUN(test_state_names);
    return TEST_SUMMARY();
}
