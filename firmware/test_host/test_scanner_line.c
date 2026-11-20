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

int main(void)
{
    RUN(test_enter_completes_line);
    RUN(test_tab_cr_percent_also_terminate);
    RUN(test_empty_line_is_ignored);
    RUN(test_input_within_guard_is_dropped);
    RUN(test_input_after_guard_is_accepted);
    RUN(test_overflow_line_is_discarded);
    RUN(test_small_out_buffer_is_rejected);
    return TEST_SUMMARY();
}
