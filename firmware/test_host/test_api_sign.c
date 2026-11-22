#include "api_sign.h"
#include "test_util.h"

static void test_take_ball_sign_input(void)
{
    /* 與 HttpCmdLibrary.SendTakeBallCmd 相同的串接順序 */
    char out[128];
    int n = api_sign_input(out, sizeof out, "1700000000", "123456", "TGSGF_1F_M1");
    CHECK_STR(out, "salt1700000000123456TGSGF_1F_M1");
    CHECK(n == (int)strlen("salt1700000000123456TGSGF_1F_M1"));
}

static void test_confirm_sign_input_without_part2(void)
{
    char out[64];
    CHECK(api_sign_input(out, sizeof out, "1700000000", "9876", NULL) > 0);
    CHECK_STR(out, "salt17000000009876");
    CHECK(api_sign_input(out, sizeof out, "1700000000", "9876", "") > 0);
    CHECK_STR(out, "salt17000000009876");
}

static void test_sign_input_rejects_small_buffer_and_null(void)
{
    char out[8];
    CHECK(api_sign_input(out, sizeof out, "1700000000", "1", "2") == -1);
    CHECK(api_sign_input(out, sizeof out, NULL, "1", "2") == -1);
    CHECK(api_sign_input(NULL, 0, "1", "1", "2") == -1);
}

static void test_form_encode_matches_python_requests(void)
{
    /* Python: urlencode({'note':'takeBall30','status':'1'}) */
    const char *keys[] = {"note", "status"};
    const char *vals[] = {"takeBall30", "1"};
    char out[64];
    CHECK(api_form_encode(out, sizeof out, keys, vals, 2) > 0);
    CHECK_STR(out, "note=takeBall30&status=1");
}

static void test_form_encode_escapes_like_python(void)
{
    /* Python: urlencode({'note':'a b&c=d/é~-_.'}) == 'note=a+b%26c%3Dd%2F%C3%A9~-_.' */
    const char *keys[] = {"note"};
    const char *vals[] = {"a b&c=d/\xC3\xA9~-_."};
    char out[64];
    CHECK(api_form_encode(out, sizeof out, keys, vals, 1) > 0);
    CHECK_STR(out, "note=a+b%26c%3Dd%2F%C3%A9~-_.");
}

static void test_form_encode_rejects_small_buffer(void)
{
    const char *keys[] = {"device_code"};
    const char *vals[] = {"TGSGF_1F_M1"};
    char out[10];
    CHECK(api_form_encode(out, sizeof out, keys, vals, 1) == -1);
}

static void test_ping_sign_input(void)
{
    /* 同 Pi 版 SendPingCmd："salt" + time + note + device_code */
    char out[128];
    CHECK(api_sign_input(out, sizeof out, "1700000000", "192.168.1.5_P4-0.1.0", "TEST_1") > 0);
    CHECK_STR(out, "salt1700000000192.168.1.5_P4-0.1.0TEST_1");
}

int main(void)
{
    RUN(test_take_ball_sign_input);
    RUN(test_confirm_sign_input_without_part2);
    RUN(test_ping_sign_input);
    RUN(test_sign_input_rejects_small_buffer_and_null);
    RUN(test_form_encode_matches_python_requests);
    RUN(test_form_encode_escapes_like_python);
    RUN(test_form_encode_rejects_small_buffer);
    return TEST_SUMMARY();
}
