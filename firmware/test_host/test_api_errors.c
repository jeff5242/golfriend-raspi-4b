#include "api_errors.h"
#include "test_util.h"

static void test_take_ball_codes_match_python(void)
{
    CHECK_STR(api_error_text(API_CMD_TAKE_BALL, "5"), "取球碼不存在");
    CHECK_STR(api_error_text(API_CMD_TAKE_BALL, "6"), "領球碼過期，請重新產生");
    CHECK_STR(api_error_text(API_CMD_TAKE_BALL, "4"), "裝置不存在");
}

static void test_confirm_codes_differ_from_take_ball(void)
{
    CHECK_STR(api_error_text(API_CMD_TAKE_BALL_CONFIRM, "4"), "交易序號不存在");
    CHECK_STR(api_error_text(API_CMD_TAKE_BALL_CONFIRM, "5"), "已設為成功 不得重複呼叫");
}

static void test_unknown_and_null(void)
{
    CHECK_STR(api_error_text(API_CMD_TAKE_BALL, "7"), "Unknow Error");
    CHECK_STR(api_error_text(API_CMD_TAKE_BALL, ""), "Unknow Error");
    CHECK_STR(api_error_text(API_CMD_TAKE_BALL, NULL), "Unknow Error");
    CHECK_STR(api_error_text((api_cmd_t)9, "1"), "Unknow Error");
}

static void test_network_failure_is_not_unknown_error(void)
{
    /* 審查 P2：連不上、逾時、HTTP 500 不得顯示 "Unknow Error" */
    CHECK_STR(api_failure_text(API_CMD_TAKE_BALL, false, ""), "網路異常，請稍後再試");
    CHECK_STR(api_failure_text(API_CMD_TAKE_BALL_CONFIRM, false, "5"), "網路異常，請稍後再試");
}

static void test_server_rejection_uses_code_table(void)
{
    CHECK_STR(api_failure_text(API_CMD_TAKE_BALL, true, "5"), "取球碼不存在");
    CHECK_STR(api_failure_text(API_CMD_TAKE_BALL_CONFIRM, true, "5"), "已設為成功 不得重複呼叫");
    CHECK_STR(api_failure_text(API_CMD_TAKE_BALL, true, "7"), "Unknow Error");
}

int main(void)
{
    RUN(test_take_ball_codes_match_python);
    RUN(test_confirm_codes_differ_from_take_ball);
    RUN(test_unknown_and_null);
    RUN(test_network_failure_is_not_unknown_error);
    RUN(test_server_rejection_uses_code_table);
    return TEST_SUMMARY();
}
