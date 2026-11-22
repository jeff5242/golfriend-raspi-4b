#include <string.h>

#include "standby_header.h"
#include "test_util.h"

#define PART_SIZE 0x100000u

static void test_max_image_size(void)
{
    CHECK(standby_max_image_size(PART_SIZE) == PART_SIZE - sizeof(standby_header_t));
    CHECK(standby_max_image_size(sizeof(standby_header_t)) == 0);
    CHECK(standby_max_image_size(4) == 0);
}

static void test_valid_header(void)
{
    const standby_header_t h = {STANDBY_HEADER_MAGIC, 36347, 0x12345678};
    CHECK(standby_header_valid(&h, PART_SIZE));
}

static void test_blank_flash_is_invalid(void)
{
    standby_header_t h;
    memset(&h, 0xFF, sizeof h);  /* 沒寫過的 Flash */
    CHECK(!standby_header_valid(&h, PART_SIZE));
    CHECK(!standby_header_valid(NULL, PART_SIZE));
}

static void test_size_limits(void)
{
    standby_header_t h = {STANDBY_HEADER_MAGIC, 0, 0};
    CHECK(!standby_header_valid(&h, PART_SIZE));  /* 空圖 */
    h.size = PART_SIZE - sizeof(standby_header_t);
    CHECK(standby_header_valid(&h, PART_SIZE));   /* 剛好放滿 */
    h.size += 1;
    CHECK(!standby_header_valid(&h, PART_SIZE));  /* 多 1 byte */
}

static void test_matches_same_image_only(void)
{
    const standby_header_t h = {STANDBY_HEADER_MAGIC, 1000, 0xAABBCCDD};
    CHECK(standby_header_matches(&h, 1000, 0xAABBCCDD));
    CHECK(!standby_header_matches(&h, 1001, 0xAABBCCDD));
    CHECK(!standby_header_matches(&h, 1000, 0xAABBCCDE));
    const standby_header_t blank = {0xFFFFFFFFu, 1000, 0xAABBCCDD};
    CHECK(!standby_header_matches(&blank, 1000, 0xAABBCCDD));  /* magic 不對就不算相同 */
    CHECK(!standby_header_matches(NULL, 1000, 0xAABBCCDD));
}

int main(void)
{
    RUN(test_max_image_size);
    RUN(test_valid_header);
    RUN(test_blank_flash_is_invalid);
    RUN(test_size_limits);
    RUN(test_matches_same_image_only);
    return TEST_SUMMARY();
}
