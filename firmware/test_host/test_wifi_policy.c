#include "wifi_policy.h"
#include "test_util.h"

#define SEC (1000LL * 1000LL)

static void test_switches_to_backup_after_max_fail(void)
{
    wifi_policy_t p;
    wifi_policy_init(&p, 2, 3, 300 * SEC);
    CHECK(wifi_policy_on_fail(&p) == 0);
    CHECK(wifi_policy_on_fail(&p) == 0);
    CHECK(wifi_policy_on_fail(&p) == 1); /* 第 3 次失敗換備援 */
}

static void test_wraps_back_to_primary(void)
{
    wifi_policy_t p;
    wifi_policy_init(&p, 2, 1, 300 * SEC);
    CHECK(wifi_policy_on_fail(&p) == 1);
    CHECK(wifi_policy_on_fail(&p) == 0);
}

static void test_connected_resets_fail_count(void)
{
    wifi_policy_t p;
    wifi_policy_init(&p, 2, 2, 300 * SEC);
    CHECK(wifi_policy_on_fail(&p) == 0);
    wifi_policy_on_connected(&p, 0);
    CHECK(wifi_policy_on_fail(&p) == 0); /* 歸零後重新計數，還沒到 2 次 */
}

static void test_retry_primary_only_when_on_backup_long_enough(void)
{
    wifi_policy_t p;
    wifi_policy_init(&p, 2, 1, 300 * SEC);
    wifi_policy_on_connected(&p, 0);
    CHECK(!wifi_policy_should_retry_primary(&p, 1000 * SEC)); /* 在主要 SSID 上 */

    CHECK(wifi_policy_on_fail(&p) == 1);
    wifi_policy_on_connected(&p, 10 * SEC);
    CHECK(!wifi_policy_should_retry_primary(&p, 100 * SEC));
    CHECK(wifi_policy_should_retry_primary(&p, 311 * SEC));
}

static void test_single_ssid_never_switches(void)
{
    wifi_policy_t p;
    wifi_policy_init(&p, 1, 1, 300 * SEC);
    CHECK(wifi_policy_on_fail(&p) == 0);
    CHECK(wifi_policy_on_fail(&p) == 0);
}

static void test_select_primary_resets_state(void)
{
    wifi_policy_t p;
    wifi_policy_init(&p, 2, 2, 300 * SEC);
    CHECK(wifi_policy_on_fail(&p) == 0);
    CHECK(wifi_policy_on_fail(&p) == 1);
    wifi_policy_on_connected(&p, 0);
    wifi_policy_select_primary(&p);
    CHECK(p.current == 0);
    CHECK(!wifi_policy_should_retry_primary(&p, 1000 * SEC));
    /* 主要 SSID 還沒恢復時，要再失敗 2 次才回到備援 */
    CHECK(wifi_policy_on_fail(&p) == 0);
    CHECK(wifi_policy_on_fail(&p) == 1);
}

int main(void)
{
    RUN(test_select_primary_resets_state);
    RUN(test_switches_to_backup_after_max_fail);
    RUN(test_wraps_back_to_primary);
    RUN(test_connected_resets_fail_count);
    RUN(test_retry_primary_only_when_on_backup_long_enough);
    RUN(test_single_ssid_never_switches);
    return TEST_SUMMARY();
}
