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

int main(void)
{
    RUN(test_take_ball_codes_match_python);
    RUN(test_confirm_codes_differ_from_take_ball);
    RUN(test_unknown_and_null);
    return TEST_SUMMARY();
}
