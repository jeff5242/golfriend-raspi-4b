#include "app_net.h"
#include "test_util.h"

static void test_wired_wins_when_both_up(void)
{
    CHECK(app_net_active(true, true) == APP_NET_ETH);
    CHECK(app_net_active(true, false) == APP_NET_ETH);
}

static void test_wifi_is_backup(void)
{
    CHECK(app_net_active(false, true) == APP_NET_WIFI);
}

static void test_no_link(void)
{
    CHECK(app_net_active(false, false) == APP_NET_NONE);
}

int main(void)
{
    RUN(test_wired_wins_when_both_up);
    RUN(test_wifi_is_backup);
    RUN(test_no_link);
    return TEST_SUMMARY();
}
