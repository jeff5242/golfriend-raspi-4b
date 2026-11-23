#include <string.h>

#include "remote_log_format.h"
#include "test_util.h"

static remote_log_entry_t entry(const char *message)
{
    const remote_log_entry_t e = {"2026-11-23T01:20:00+0800", "INFO", REMOTE_LOG_OPERATION,
                                  "TGSGF_1F_M1", message};
    return e;
}

static void test_matches_pi_fields(void)
{
    char out[256];
    const remote_log_entry_t e = entry("take_ball ok take=50");
    const int n = remote_log_format_entry(out, sizeof out, &e);
    CHECK_STR(out, "{\"dt\":\"2026-11-23T01:20:00+0800\",\"level\":\"INFO\","
                   "\"message\":\"[TGSGF_1F_M1] [操作] take_ball ok take=50\","
                   "\"device\":\"TGSGF_1F_M1\",\"tag\":\"operation\"}");
    CHECK(n == (int)strlen(out));
}

static void test_tag_names(void)
{
    char out[256];
    remote_log_entry_t e = entry("x");
    e.tag = REMOTE_LOG_SYSTEM;
    remote_log_format_entry(out, sizeof out, &e);
    CHECK(strstr(out, "[系統] x") != NULL && strstr(out, "\"tag\":\"system\"") != NULL);
    e.tag = REMOTE_LOG_EXCEPTION;
    remote_log_format_entry(out, sizeof out, &e);
    CHECK(strstr(out, "[例外] x") != NULL && strstr(out, "\"tag\":\"exception\"") != NULL);
    e.tag = REMOTE_LOG_INFO;
    remote_log_format_entry(out, sizeof out, &e);
    CHECK(strstr(out, "[資訊] x") != NULL && strstr(out, "\"tag\":\"info\"") != NULL);
}

static void test_escapes_json_specials(void)
{
    char out[256];
    const remote_log_entry_t e = entry("resp {\"error\":true} path C:\\x\nline2\ttab\x01");
    CHECK(remote_log_format_entry(out, sizeof out, &e) > 0);
    CHECK(strstr(out, "resp {\\\"error\\\":true} path C:\\\\x\\nline2\\ttab\\u0001") != NULL);
}

static void test_keeps_utf8(void)
{
    char out[256];
    const remote_log_entry_t e = entry("領球碼過期，請重新產生");
    CHECK(remote_log_format_entry(out, sizeof out, &e) > 0);
    CHECK(strstr(out, "領球碼過期，請重新產生") != NULL);
}

static void test_rejects_small_buffer_and_null(void)
{
    char out[40];
    const remote_log_entry_t e = entry("0123456789012345678901234567890123456789");
    CHECK(remote_log_format_entry(out, sizeof out, &e) == -1);
    CHECK(remote_log_format_entry(NULL, 10, &e) == -1);
    CHECK(remote_log_format_entry(out, sizeof out, NULL) == -1);
    remote_log_entry_t bad = e;
    bad.message = NULL;
    char big[256];
    CHECK(remote_log_format_entry(big, sizeof big, &bad) == -1);
}

int main(void)
{
    RUN(test_matches_pi_fields);
    RUN(test_tag_names);
    RUN(test_escapes_json_specials);
    RUN(test_keeps_utf8);
    RUN(test_rejects_small_buffer_and_null);
    return TEST_SUMMARY();
}
