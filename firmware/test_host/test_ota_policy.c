#include "ota_policy.h"
#include "test_util.h"

static void test_newer_versions(void)
{
    CHECK(ota_is_newer("P4-0.1.0", "P4-0.1.1"));
    CHECK(ota_is_newer("P4-0.1.9", "P4-0.2.0"));
    CHECK(ota_is_newer("P4-0.9.9", "P4-1.0.0"));
    CHECK(ota_is_newer("P4-0.1.9", "P4-0.1.10"));  /* 數字比較，不是字串比較 */
}

static void test_same_or_older_is_not_newer(void)
{
    CHECK(!ota_is_newer("P4-0.1.1", "P4-0.1.1"));
    CHECK(!ota_is_newer("P4-0.1.1", "P4-0.1.0"));
    CHECK(!ota_is_newer("P4-1.0.0", "P4-0.9.9"));
    CHECK(!ota_is_newer("P4-0.1.10", "P4-0.1.9"));
}

static void test_other_board_is_rejected(void)
{
    CHECK(!ota_is_newer("P4-0.1.0", "S3-9.9.9"));
    CHECK(!ota_is_newer("P4-0.1.0", "P40-0.1.1"));
}

static void test_malformed_versions_are_rejected(void)
{
    CHECK(!ota_is_newer("P4-0.1.0", ""));
    CHECK(!ota_is_newer("P4-0.1.0", "0.1.1"));          /* 沒有機型 */
    CHECK(!ota_is_newer("P4-0.1.0", "P4-0.1"));         /* 少一段 */
    CHECK(!ota_is_newer("P4-0.1.0", "P4-0.1.1.2"));     /* 多一段 */
    CHECK(!ota_is_newer("P4-0.1.0", "P4-0.1.x"));
    CHECK(!ota_is_newer("P4-0.1.0", "P4-0.1.1-dirty")); /* 開發中的版本不發布 */
    CHECK(!ota_is_newer("P4-0.1.0", "P4--1.0.0"));
    CHECK(!ota_is_newer("c35d355-dirty", "P4-0.1.1"));  /* 目前版本看不懂時不更新 */
    CHECK(!ota_is_newer(NULL, "P4-0.1.1"));
    CHECK(!ota_is_newer("P4-0.1.0", NULL));
}

static void test_url_scheme(void)
{
    CHECK(ota_url_allowed("https://example.com/fw.bin", false));
    CHECK(!ota_url_allowed("http://example.com/fw.bin", false));
    CHECK(ota_url_allowed("http://192.168.1.5:8000/fw.bin", true));
    CHECK(ota_url_allowed("https://example.com/fw.bin", true));
    CHECK(!ota_url_allowed("ftp://example.com/fw.bin", true));
    CHECK(!ota_url_allowed("https://", false));  /* 沒有主機名稱 */
    CHECK(!ota_url_allowed("", true));
    CHECK(!ota_url_allowed(NULL, true));
}

int main(void)
{
    RUN(test_newer_versions);
    RUN(test_same_or_older_is_not_newer);
    RUN(test_other_board_is_rejected);
    RUN(test_malformed_versions_are_rejected);
    RUN(test_url_scheme);
    return TEST_SUMMARY();
}
