#include "scanner_line.h"
#include "test_util.h"

/* 把整串字元依序送入，回傳最後一次完成的行數；完成的行寫到 out。 */
static int feed_all(scanner_line_t *l, const char *s, int64_t t0_us, int64_t step_us,
                    char *out)
{
    int done = 0;
    for (size_t i = 0; s[i] != '\0'; i++) {
        if (scanner_line_feed(l, s[i], t0_us + (int64_t)i * step_us, out, SCANNER_LINE_MAX)) {
            done++;
        }
    }
    return done;
}

static void test_enter_completes_line(void)
{
    scanner_line_t l;
    char out[SCANNER_LINE_MAX] = "";
    scanner_line_init(&l);
    CHECK(feed_all(&l, "123456\n", 0, 1000, out) == 1);
    CHECK_STR(out, "123456");
}

static void test_tab_cr_percent_also_terminate(void)
{
    const char *cases[] = {"111\t", "222\r", "333%"};
    const char *want[] = {"111", "222", "333"};
    for (int i = 0; i < 3; i++) {
        scanner_line_t l;
        char out[SCANNER_LINE_MAX] = "";
        scanner_line_init(&l);
        CHECK(feed_all(&l, cases[i], 0, 1000, out) == 1);
        CHECK_STR(out, want[i]);
    }
}

static void test_empty_line_is_ignored(void)
{
    scanner_line_t l;
    char out[SCANNER_LINE_MAX] = "";
    scanner_line_init(&l);
    CHECK(feed_all(&l, "\n\n", 0, 1000, out) == 0);
}

static void test_input_within_guard_is_dropped(void)
{
    /* 掃碼器殘留的 Enter 落在 1 秒內，不得再產生一行，也不得帶進下一行 */
    scanner_line_t l;
    char out[SCANNER_LINE_MAX] = "";
    scanner_line_init(&l);
    CHECK(feed_all(&l, "123\n", 0, 1000, out) == 1);
    CHECK(!scanner_line_feed(&l, '9', 500 * 1000, out, sizeof out));
    CHECK(!scanner_line_feed(&l, '\n', 600 * 1000, out, sizeof out));
    CHECK_STR(out, "123");
}

static void test_input_after_guard_is_accepted(void)
{
    scanner_line_t l;
    char out[SCANNER_LINE_MAX] = "";
    scanner_line_init(&l);
    CHECK(feed_all(&l, "123\n", 0, 1000, out) == 1);
    CHECK(feed_all(&l, "456\n", 2 * 1000 * 1000, 1000, out) == 1);
    CHECK_STR(out, "456");
}

static void test_overflow_line_is_discarded(void)
{
    scanner_line_t l;
    char out[SCANNER_LINE_MAX] = "unchanged";
    char longline[SCANNER_LINE_MAX + 10];
    memset(longline, '7', sizeof longline - 2);
    longline[sizeof longline - 2] = '\n';
    longline[sizeof longline - 1] = '\0';
    scanner_line_init(&l);
    CHECK(feed_all(&l, longline, 0, 1000, out) == 0);
    CHECK_STR(out, "unchanged");
    /* 丟掉之後，下一行要能正常收 */
    CHECK(feed_all(&l, "88\n", 0, 1000, out) == 1);
    CHECK_STR(out, "88");
}

static void test_small_out_buffer_is_rejected(void)
{
    scanner_line_t l;
    char out[4] = "";
    scanner_line_init(&l);
    CHECK(!scanner_line_feed(&l, '1', 0, out, sizeof out));
    CHECK(!scanner_line_feed(&l, '\n', 1, out, sizeof out));
}

static void test_clear_drops_unfinished_input(void)
{
    /* 審查 P1：客人打了 "12" 沒按 Enter，逾時回待機後，下一位掃 "98765" 不得變成 "1298765" */
    scanner_line_t l;
    char out[SCANNER_LINE_MAX] = "";
    scanner_line_init(&l);
    CHECK(feed_all(&l, "12", 0, 1000, out) == 0);
    CHECK(scanner_line_clear_stale(&l, 11 * 1000 * 1000));
    CHECK(feed_all(&l, "98765\n", 11 * 1000 * 1000, 1000, out) == 1);
    CHECK_STR(out, "98765");
}

static void test_clear_keeps_key_guard(void)
{
    /* 清掉殘留字元不得順便取消 1 秒按鍵保護 */
    scanner_line_t l;
    char out[SCANNER_LINE_MAX] = "";
    scanner_line_init(&l);
    CHECK(feed_all(&l, "123\n", 0, 1000, out) == 1);
    scanner_line_clear_stale(&l, 400 * 1000);
    CHECK(!scanner_line_feed(&l, '9', 500 * 1000, out, sizeof out));
    CHECK(!scanner_line_feed(&l, '\n', 600 * 1000, out, sizeof out));
    CHECK_STR(out, "123");
}

static void test_clear_resets_overflow(void)
{
    scanner_line_t l;
    char out[SCANNER_LINE_MAX] = "";
    char longline[SCANNER_LINE_MAX + 5];
    memset(longline, '7', sizeof longline - 1);
    longline[sizeof longline - 1] = '\0';
    scanner_line_init(&l);
    CHECK(feed_all(&l, longline, 0, 1000, out) == 0);
    CHECK(scanner_line_clear_stale(&l, 2 * 1000 * 1000));
    CHECK(feed_all(&l, "55\n", 2 * 1000 * 1000, 1000, out) == 1);
    CHECK_STR(out, "55");
}

static void test_clear_keeps_scan_in_progress(void)
{
    /* 審查 P2：下一位在出球結束那一刻掃碼，"987" 已進來時回待機，不得只剩 "65" */
    scanner_line_t l;
    char out[SCANNER_LINE_MAX] = "";
    scanner_line_init(&l);
    CHECK(feed_all(&l, "987", 0, 5000, out) == 0);          /* 0、5、10 ms */
    CHECK(!scanner_line_clear_stale(&l, 15 * 1000));        /* 距最後一字 5 ms */
    CHECK(feed_all(&l, "65\n", 15 * 1000, 5000, out) == 1);
    CHECK_STR(out, "98765");
}

static void test_clear_stale_boundary(void)
{
    scanner_line_t l;
    char out[SCANNER_LINE_MAX] = "";
    scanner_line_init(&l);
    CHECK(feed_all(&l, "12", 0, 1000, out) == 0);           /* 最後一字在 1 ms */
    CHECK(!scanner_line_clear_stale(&l, 1000 + SCANNER_IDLE_CLEAR_US - 1));
    CHECK(scanner_line_clear_stale(&l, 1000 + SCANNER_IDLE_CLEAR_US));
    CHECK(feed_all(&l, "3\n", 2 * 1000 * 1000, 1000, out) == 1);
    CHECK_STR(out, "3");
}

int main(void)
{
    RUN(test_enter_completes_line);
    RUN(test_tab_cr_percent_also_terminate);
    RUN(test_empty_line_is_ignored);
    RUN(test_input_within_guard_is_dropped);
    RUN(test_input_after_guard_is_accepted);
    RUN(test_overflow_line_is_discarded);
    RUN(test_small_out_buffer_is_rejected);
    RUN(test_clear_drops_unfinished_input);
    RUN(test_clear_keeps_key_guard);
    RUN(test_clear_resets_overflow);
    RUN(test_clear_keeps_scan_in_progress);
    RUN(test_clear_stale_boundary);
    return TEST_SUMMARY();
}
