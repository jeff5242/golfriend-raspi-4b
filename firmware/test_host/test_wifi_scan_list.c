#include "wifi_scan_list.h"
#include "test_util.h"

static wifi_scan_ap_t ap(const char *ssid, int rssi, bool secured)
{
    wifi_scan_ap_t a = {.rssi = (int8_t)rssi, .secured = secured};
    strncpy(a.ssid, ssid, sizeof a.ssid - 1);
    return a;
}

static void test_sorted_by_signal(void)
{
    wifi_scan_ap_t aps[] = {ap("weak", -80, true), ap("strong", -40, true), ap("mid", -60, false)};
    const size_t n = wifi_scan_list_normalize(aps, 3, WIFI_SCAN_LIST_MAX);
    CHECK(n == 3);
    CHECK_STR(aps[0].ssid, "strong");
    CHECK_STR(aps[1].ssid, "mid");
    CHECK_STR(aps[2].ssid, "weak");
    CHECK(aps[1].secured == false);
}

static void test_hidden_ssid_dropped(void)
{
    wifi_scan_ap_t aps[] = {ap("", -30, true), ap("golfriend", -50, true)};
    const size_t n = wifi_scan_list_normalize(aps, 2, WIFI_SCAN_LIST_MAX);
    CHECK(n == 1);
    CHECK_STR(aps[0].ssid, "golfriend");
}

static void test_same_name_keeps_strongest(void)
{
    /* 練習場常有多台同名 AP：只列一次，訊號用最強那台 */
    wifi_scan_ap_t aps[] = {ap("golfriend", -70, true), ap("other", -60, true),
                            ap("golfriend", -45, true), ap("golfriend", -90, true)};
    const size_t n = wifi_scan_list_normalize(aps, 4, WIFI_SCAN_LIST_MAX);
    CHECK(n == 2);
    CHECK_STR(aps[0].ssid, "golfriend");
    CHECK(aps[0].rssi == -45);
    CHECK_STR(aps[1].ssid, "other");
}

static void test_capped_at_max(void)
{
    wifi_scan_ap_t aps[] = {ap("a", -90, true), ap("b", -40, true), ap("c", -50, true)};
    const size_t n = wifi_scan_list_normalize(aps, 3, 2);
    CHECK(n == 2);
    CHECK_STR(aps[0].ssid, "b");  /* 截掉的是最弱的 */
    CHECK_STR(aps[1].ssid, "c");
}

static void test_equal_signal_keeps_scan_order(void)
{
    wifi_scan_ap_t aps[] = {ap("first", -60, true), ap("second", -60, true)};
    wifi_scan_list_normalize(aps, 2, WIFI_SCAN_LIST_MAX);
    CHECK_STR(aps[0].ssid, "first");
    CHECK_STR(aps[1].ssid, "second");
}

static void test_unterminated_ssid_is_cut(void)
{
    /* 32 bytes 的 SSID 在 wifi_ap_record_t 裡不一定有結尾 0 */
    wifi_scan_ap_t aps[1];
    memset(aps[0].ssid, 'x', sizeof aps[0].ssid);
    aps[0].rssi = -50;
    aps[0].secured = true;
    const size_t n = wifi_scan_list_normalize(aps, 1, WIFI_SCAN_LIST_MAX);
    CHECK(n == 1);
    CHECK(strlen(aps[0].ssid) == WIFI_SCAN_SSID_MAX - 1);
}

static void test_empty_and_null(void)
{
    CHECK(wifi_scan_list_normalize(NULL, 3, WIFI_SCAN_LIST_MAX) == 0);
    wifi_scan_ap_t aps[1];
    CHECK(wifi_scan_list_normalize(aps, 0, WIFI_SCAN_LIST_MAX) == 0);
}

static void test_reasons(void)
{
    CHECK(wifi_reason_is_auth_failure(15));   /* 4WAY_HANDSHAKE_TIMEOUT：密碼錯 */
    CHECK(wifi_reason_is_auth_failure(204));  /* HANDSHAKE_TIMEOUT */
    CHECK(wifi_reason_is_auth_failure(202));  /* AUTH_FAIL */
    CHECK(wifi_reason_is_auth_failure(2));    /* AUTH_EXPIRE */
    CHECK(!wifi_reason_is_auth_failure(201));
    CHECK(!wifi_reason_is_auth_failure(8));   /* 自己斷線 */
    CHECK(wifi_reason_is_not_found(201));
    CHECK(!wifi_reason_is_not_found(15));
}

int main(void)
{
    RUN(test_sorted_by_signal);
    RUN(test_hidden_ssid_dropped);
    RUN(test_same_name_keeps_strongest);
    RUN(test_capped_at_max);
    RUN(test_equal_signal_keeps_scan_order);
    RUN(test_unterminated_ssid_is_cut);
    RUN(test_empty_and_null);
    RUN(test_reasons);
    return TEST_SUMMARY();
}
