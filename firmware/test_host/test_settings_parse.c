#include <string.h>

#include "settings_parse.h"
#include "test_util.h"

static settings_t defaults(void)
{
    settings_t s;
    memset(&s, 0, sizeof s);
    strcpy(s.device_code, "OLD_CODE");
    strcpy(s.wifi1_ssid, "old-ssid");
    strcpy(s.wifi1_password, "old-pass");
    return s;
}

static void test_parses_all_keys(void)
{
    settings_t s = defaults();
    const settings_parse_result_t r = settings_parse(
        "device_code=TGSGF_1F_M1\nwifi1_ssid=golf\nwifi1_password=abc 123\n"
        "wifi2_ssid=backup\nwifi2_password=p@ss=word\n", &s);
    CHECK(r.applied == 5);
    CHECK(r.rejected == 0);
    CHECK_STR(s.device_code, "TGSGF_1F_M1");
    CHECK_STR(s.wifi1_ssid, "golf");
    CHECK_STR(s.wifi1_password, "abc 123");   /* 值中間的空白保留 */
    CHECK_STR(s.wifi2_ssid, "backup");
    CHECK_STR(s.wifi2_password, "p@ss=word"); /* 只在第一個 '=' 切開 */
}

static void test_missing_keys_keep_old_values(void)
{
    settings_t s = defaults();
    const settings_parse_result_t r = settings_parse("device_code=NEW\n", &s);
    CHECK(r.applied == 1);
    CHECK_STR(s.device_code, "NEW");
    CHECK_STR(s.wifi1_ssid, "old-ssid");
    CHECK_STR(s.wifi1_password, "old-pass");
}

static void test_tolerates_windows_file(void)
{
    /* 記事本存的檔：UTF-8 BOM、CRLF、前後空白、空行、註解、最後一行沒有換行 */
    settings_t s = defaults();
    const settings_parse_result_t r = settings_parse(
        "\xEF\xBB\xBF# 松柏 1 樓\r\n\r\n  device_code = SB_1F_M1  \r\n"
        "wifi1_ssid\t=\tgolf\r\n#wifi1_password=ignored\r\nwifi2_ssid=last", &s);
    CHECK(r.applied == 3);
    CHECK(r.rejected == 0);
    CHECK_STR(s.device_code, "SB_1F_M1");
    CHECK_STR(s.wifi1_ssid, "golf");
    CHECK_STR(s.wifi1_password, "old-pass");
    CHECK_STR(s.wifi2_ssid, "last");
}

static void test_empty_value_clears_item(void)
{
    /* 寫 wifi2_ssid= 代表拿掉備援 WiFi */
    settings_t s = defaults();
    strcpy(s.wifi2_ssid, "backup");
    const settings_parse_result_t r = settings_parse("wifi2_ssid=\n", &s);
    CHECK(r.applied == 1);
    CHECK_STR(s.wifi2_ssid, "");
}

static void test_rejects_bad_lines_without_touching_values(void)
{
    settings_t s = defaults();
    char long_line[160] = "device_code=";
    memset(long_line + strlen(long_line), 'A', SETTINGS_DEVICE_CODE_MAX);  /* 超過 1 個字 */
    strcat(long_line, "\nno_equal_sign\nunknown_key=1\n=novalue\n");
    const settings_parse_result_t r = settings_parse(long_line, &s);
    CHECK(r.applied == 0);
    CHECK(r.rejected == 4);
    CHECK_STR(s.device_code, "OLD_CODE");  /* 太長的值不截斷、不套用 */
}

static void test_longest_allowed_value_fits(void)
{
    settings_t s = defaults();
    char line[160] = "wifi1_password=";
    memset(line + strlen(line), 'x', SETTINGS_PASSWORD_MAX - 1);
    const settings_parse_result_t r = settings_parse(line, &s);
    CHECK(r.applied == 1);
    CHECK(strlen(s.wifi1_password) == SETTINGS_PASSWORD_MAX - 1);
}

static void test_null_arguments(void)
{
    settings_t s = defaults();
    CHECK(settings_parse(NULL, &s).applied == 0);
    CHECK(settings_parse("device_code=X\n", NULL).applied == 0);
    CHECK(settings_parse("", &s).applied == 0);
}

static void test_log_token(void)
{
    settings_t s = defaults();
    CHECK(settings_parse("log_token=abcDEF123\n", &s).applied == 1);
    CHECK_STR(s.log_token, "abcDEF123");
}

int main(void)
{
    RUN(test_parses_all_keys);
    RUN(test_missing_keys_keep_old_values);
    RUN(test_tolerates_windows_file);
    RUN(test_empty_value_clears_item);
    RUN(test_rejects_bad_lines_without_touching_values);
    RUN(test_longest_allowed_value_fits);
    RUN(test_log_token);
    RUN(test_null_arguments);
    return TEST_SUMMARY();
}
