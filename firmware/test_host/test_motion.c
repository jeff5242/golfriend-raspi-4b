#include "motion.h"
#include "test_util.h"

#define W 160
#define H 90  /* 每格 10x10 */

static uint8_t img[W * H];

static void fill(uint8_t v)
{
    memset(img, v, sizeof img);
}

/* 把第 (bx, by) 格整格填成 v */
static void paint_block(int bx, int by, uint8_t v)
{
    for (int y = by * 10; y < (by + 1) * 10; y++) {
        memset(img + y * W + bx * 10, v, 10);
    }
}

static void test_first_frame_is_not_motion(void)
{
    motion_t m = {0};
    fill(100);
    CHECK(!motion_detect(&m, img, W, H));
    CHECK(!motion_detect(&m, img, W, H));  /* 同樣的畫面 */
}

static void test_object_entering_is_motion(void)
{
    motion_t m = {0};
    fill(100);
    motion_detect(&m, img, W, H);
    for (int i = 0; i < 4; i++) {
        paint_block(6 + i, 4, 220);  /* 手機螢幕進到畫面中央 */
    }
    CHECK(motion_detect(&m, img, W, H));
    CHECK(!motion_detect(&m, img, W, H));  /* 停住不動：和上一張相同 */
}

static void test_small_change_is_ignored(void)
{
    motion_t m = {0};
    fill(100);
    motion_detect(&m, img, W, H);
    paint_block(0, 0, 220);
    paint_block(15, 8, 220);  /* 只有 2 格變：雜訊或遠處的小東西 */
    CHECK(!motion_detect(&m, img, W, H));
}

static void test_tiny_brightness_change_is_ignored(void)
{
    motion_t m = {0};
    fill(100);
    motion_detect(&m, img, W, H);
    for (int i = 0; i < 10; i++) {
        paint_block(i, 2, 100 + MOTION_BLOCK_DIFF);  /* 差剛好等於門檻：不算 */
    }
    CHECK(!motion_detect(&m, img, W, H));
}

static void test_global_exposure_change_is_ignored(void)
{
    /* 自動曝光把整張調亮：每一格一起變，不是有人在動 */
    motion_t m = {0};
    fill(80);
    paint_block(3, 3, 30);
    motion_detect(&m, img, W, H);
    fill(140);
    paint_block(3, 3, 90);
    CHECK(!motion_detect(&m, img, W, H));
}

static void test_too_small_image(void)
{
    motion_t m = {0};
    CHECK(!motion_detect(&m, img, MOTION_GRID_X - 1, H));
    CHECK(!motion_detect(NULL, img, W, H));
    CHECK(!motion_detect(&m, NULL, W, H));
}

static void test_activity_shows_then_hides_after_hold(void)
{
    motion_t m = {0};
    CHECK(!motion_activity(&m, false, 1000, 20000));  /* 開機沒動靜：不顯示 */
    CHECK(motion_activity(&m, true, 2000, 20000));
    CHECK(motion_activity(&m, false, 2000 + 20000, 20000));   /* 剛好 20 秒：還顯示 */
    CHECK(!motion_activity(&m, false, 2000 + 20001, 20000));  /* 超過：隱藏 */
}

static void test_activity_restarts_hold(void)
{
    motion_t m = {0};
    motion_activity(&m, true, 0, 20000);
    motion_activity(&m, true, 15000, 20000);  /* 15 秒時又有動靜 */
    CHECK(motion_activity(&m, false, 30000, 20000));
    CHECK(!motion_activity(&m, false, 35001, 20000));
}

static void test_activity_wraps_around(void)
{
    motion_t m = {0};
    motion_activity(&m, true, 0xFFFFF000u, 20000);
    CHECK(motion_activity(&m, false, 0x00000100u, 20000));  /* 繞回後約 4.3 秒 */
    CHECK(!motion_activity(&m, false, 0x00005000u, 20000)); /* 繞回後約 24 秒 */
}

int main(void)
{
    RUN(test_first_frame_is_not_motion);
    RUN(test_object_entering_is_motion);
    RUN(test_small_change_is_ignored);
    RUN(test_tiny_brightness_change_is_ignored);
    RUN(test_global_exposure_change_is_ignored);
    RUN(test_too_small_image);
    RUN(test_activity_shows_then_hides_after_hold);
    RUN(test_activity_restarts_hold);
    RUN(test_activity_wraps_around);
    return TEST_SUMMARY();
}
