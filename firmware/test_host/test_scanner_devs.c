#include "scanner_devs.h"
#include "test_util.h"

#define MS 1000LL

/* HID usage ID：'1'..'9' = 0x1E..0x26，'0' = 0x27，Enter = 0x28 */
static uint8_t usage_of(char c)
{
    if (c == '\n') {
        return 0x28;
    }
    if (c == '0') {
        return 0x27;
    }
    return (uint8_t)(0x1E + (c - '1'));
}

static void make_report(uint8_t report[8], uint8_t usage)
{
    memset(report, 0, 8);
    report[2] = usage;
}

/* 一顆鍵「按下」：回傳新按下的字元數，字元寫到 out。 */
static size_t press(scanner_dev_t *dev, char c, char out[HID_KBD_KEY_SLOTS])
{
    uint8_t report[8];
    make_report(report, usage_of(c));
    return scanner_dev_report_chars(dev, report, sizeof report, out);
}

static void release(scanner_dev_t *dev)
{
    uint8_t report[8];
    char out[HID_KBD_KEY_SLOTS];
    make_report(report, 0);
    (void)scanner_dev_report_chars(dev, report, sizeof report, out);
}

/* 按下並放開一顆鍵，把字元送進該裝置的行緩衝。完成一行時回傳 true。 */
static bool type_key(scanner_dev_t *dev, char c, int64_t now_us, uint32_t gen, char *line)
{
    char chars[HID_KBD_KEY_SLOTS];
    bool done = false;
    const size_t n = press(dev, c, chars);
    for (size_t i = 0; i < n; i++) {
        done = scanner_dev_feed(dev, chars[i], now_us, gen, line, SCANNER_LINE_MAX) || done;
    }
    release(dev);
    return done;
}

static int dev_a, dev_b, dev_c, dev_d, dev_e;

static void test_two_devices_interleaved_keep_own_lines(void)
{
    /* 審查 P2：讀卡機和掃碼器同時送字，兩邊的字不得混成一行 */
    scanner_devs_t devs;
    char line_a[SCANNER_LINE_MAX] = "";
    char line_b[SCANNER_LINE_MAX] = "";
    scanner_devs_init(&devs);
    scanner_dev_t *a = scanner_devs_get(&devs, &dev_a, 0);
    scanner_dev_t *b = scanner_devs_get(&devs, &dev_b, 0);
    CHECK(a != NULL && b != NULL && a != b);
    CHECK(!type_key(a, '1', 0 * MS, 0, line_a));
    CHECK(!type_key(b, '9', 1 * MS, 0, line_b));
    CHECK(!type_key(a, '2', 2 * MS, 0, line_a));
    CHECK(!type_key(b, '8', 3 * MS, 0, line_b));
    CHECK(type_key(a, '\n', 4 * MS, 0, line_a));
    CHECK(type_key(b, '\n', 5 * MS, 0, line_b));
    CHECK_STR(line_a, "12");
    CHECK_STR(line_b, "98");
}

static void test_same_key_on_two_devices_both_count(void)
{
    /* 共用「上一份報告」時，B 按住和 A 相同的鍵會被當成沒放開而漏字 */
    scanner_devs_t devs;
    char out[HID_KBD_KEY_SLOTS];
    scanner_devs_init(&devs);
    scanner_dev_t *a = scanner_devs_get(&devs, &dev_a, 0);
    scanner_dev_t *b = scanner_devs_get(&devs, &dev_b, 0);
    CHECK(press(a, '5', out) == 1);
    CHECK(press(b, '5', out) == 1);
    CHECK(out[0] == '5');
}

static void test_get_returns_same_slot_for_same_device(void)
{
    scanner_devs_t devs;
    scanner_devs_init(&devs);
    scanner_dev_t *a1 = scanner_devs_get(&devs, &dev_a, 0);
    scanner_dev_t *a2 = scanner_devs_get(&devs, &dev_a, 0);
    CHECK(a1 == a2);
    CHECK(scanner_devs_get(&devs, NULL, 0) == NULL);
}

static void test_remove_clears_only_that_device(void)
{
    scanner_devs_t devs;
    char line_a[SCANNER_LINE_MAX] = "";
    char line_b[SCANNER_LINE_MAX] = "";
    scanner_devs_init(&devs);
    scanner_dev_t *a = scanner_devs_get(&devs, &dev_a, 0);
    scanner_dev_t *b = scanner_devs_get(&devs, &dev_b, 0);
    CHECK(!type_key(a, '1', 0, 0, line_a));
    CHECK(!type_key(b, '9', 1 * MS, 0, line_b));
    scanner_devs_remove(&devs, &dev_a);             /* A 拔掉 */
    a = scanner_devs_get(&devs, &dev_a, 0);          /* A 重新插上：從空行開始 */
    CHECK(!type_key(a, '2', 2 * MS, 0, line_a));
    CHECK(type_key(a, '\n', 3 * MS, 0, line_a));
    CHECK_STR(line_a, "2");
    CHECK(type_key(b, '\n', 4 * MS, 0, line_b));
    CHECK_STR(line_b, "9");
}

static void test_reset_clears_each_device_only_when_idle(void)
{
    /* 回待機（reset_gen 加 1）：閒置的殘字清掉，正在掃的碼保留 */
    scanner_devs_t devs;
    char line_a[SCANNER_LINE_MAX] = "";
    char line_b[SCANNER_LINE_MAX] = "";
    scanner_devs_init(&devs);
    scanner_dev_t *a = scanner_devs_get(&devs, &dev_a, 0);
    scanner_dev_t *b = scanner_devs_get(&devs, &dev_b, 0);
    CHECK(!type_key(a, '1', 0, 0, line_a));                  /* A：手動打一半就走了 */
    CHECK(!type_key(b, '9', 10000 * MS, 0, line_b));         /* B：掃碼中 */
    CHECK(!type_key(b, '8', 10005 * MS, 0, line_b));
    /* 10006 ms 回待機，gen 0 → 1 */
    CHECK(!type_key(b, '7', 10010 * MS, 1, line_b));
    CHECK(type_key(b, '\n', 10015 * MS, 1, line_b));
    CHECK_STR(line_b, "987");
    CHECK(!type_key(a, '5', 11000 * MS, 1, line_a));
    CHECK(type_key(a, '\n', 11001 * MS, 1, line_a));
    CHECK_STR(line_a, "5");
}

static void test_new_device_after_reset_is_not_cleared(void)
{
    scanner_devs_t devs;
    char line[SCANNER_LINE_MAX] = "";
    scanner_devs_init(&devs);
    scanner_dev_t *a = scanner_devs_get(&devs, &dev_a, 7);
    CHECK(!type_key(a, '4', 0, 7, line));
    CHECK(type_key(a, '\n', 1 * MS, 7, line));
    CHECK_STR(line, "4");
}

static void test_table_full_returns_null(void)
{
    scanner_devs_t devs;
    scanner_devs_init(&devs);
    CHECK(scanner_devs_get(&devs, &dev_a, 0) != NULL);
    CHECK(scanner_devs_get(&devs, &dev_b, 0) != NULL);
    CHECK(scanner_devs_get(&devs, &dev_c, 0) != NULL);
    CHECK(scanner_devs_get(&devs, &dev_d, 0) != NULL);
    CHECK(scanner_devs_get(&devs, &dev_e, 0) == NULL);
    scanner_devs_remove(&devs, &dev_b);
    CHECK(scanner_devs_get(&devs, &dev_e, 0) != NULL);
}

static void test_short_report_ignored(void)
{
    scanner_devs_t devs;
    char out[HID_KBD_KEY_SLOTS];
    const uint8_t report[4] = {0, 0, 0x1E, 0};
    scanner_devs_init(&devs);
    scanner_dev_t *a = scanner_devs_get(&devs, &dev_a, 0);
    CHECK(scanner_dev_report_chars(a, report, sizeof report, out) == 0);
    CHECK(scanner_dev_report_chars(a, NULL, 8, out) == 0);
}

int main(void)
{
    RUN(test_two_devices_interleaved_keep_own_lines);
    RUN(test_same_key_on_two_devices_both_count);
    RUN(test_get_returns_same_slot_for_same_device);
    RUN(test_remove_clears_only_that_device);
    RUN(test_reset_clears_each_device_only_when_idle);
    RUN(test_new_device_after_reset_is_not_cleared);
    RUN(test_table_full_returns_null);
    RUN(test_short_report_ignored);
    return TEST_SUMMARY();
}
