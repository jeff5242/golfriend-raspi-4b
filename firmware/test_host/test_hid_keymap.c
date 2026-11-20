#include "hid_keymap.h"
#include "test_util.h"

static void test_digits_and_letters(void)
{
    CHECK(hid_keymap_to_ascii(0x1E, false) == '1');
    CHECK(hid_keymap_to_ascii(0x26, false) == '9');
    CHECK(hid_keymap_to_ascii(0x27, false) == '0');
    CHECK(hid_keymap_to_ascii(0x04, false) == 'a');
    CHECK(hid_keymap_to_ascii(0x1D, false) == 'z');
    CHECK(hid_keymap_to_ascii(0x04, true) == 'A');
}

static void test_shift_symbols(void)
{
    /* Pi 版用 '%' 當結束字元，掃碼器送的是 Shift + 5 */
    CHECK(hid_keymap_to_ascii(0x22, true) == '%');
    CHECK(hid_keymap_to_ascii(0x1E, true) == '!');
    CHECK(hid_keymap_to_ascii(0x27, true) == ')');
    CHECK(hid_keymap_to_ascii(0x2D, false) == '-');
    CHECK(hid_keymap_to_ascii(0x2D, true) == '_');
}

static void test_control_keys(void)
{
    CHECK(hid_keymap_to_ascii(0x28, false) == '\n'); /* Enter */
    CHECK(hid_keymap_to_ascii(0x58, false) == '\n'); /* 數字鍵盤 Enter */
    CHECK(hid_keymap_to_ascii(0x2B, false) == '\t'); /* Tab */
    CHECK(hid_keymap_to_ascii(0x2C, false) == ' ');
}

static void test_keypad_digits(void)
{
    /* 部分讀卡機用數字鍵盤送卡號 */
    CHECK(hid_keymap_to_ascii(0x59, false) == '1');
    CHECK(hid_keymap_to_ascii(0x61, false) == '9');
    CHECK(hid_keymap_to_ascii(0x62, false) == '0');
}

static void test_unsupported_returns_zero(void)
{
    CHECK(hid_keymap_to_ascii(0x00, false) == 0);
    CHECK(hid_keymap_to_ascii(0x01, false) == 0); /* ErrorRollOver */
    CHECK(hid_keymap_to_ascii(0x3A, false) == 0); /* F1 */
    CHECK(hid_keymap_to_ascii(0xFF, false) == 0);
}

static void test_shift_modifier_bits(void)
{
    CHECK(hid_keymap_has_shift(0x02));  /* 左 Shift */
    CHECK(hid_keymap_has_shift(0x20));  /* 右 Shift */
    CHECK(!hid_keymap_has_shift(0x01)); /* 左 Ctrl */
    CHECK(!hid_keymap_has_shift(0x00));
}

static void test_new_keys_ignores_held_keys(void)
{
    const uint8_t prev[6] = {0x1E, 0, 0, 0, 0, 0};
    const uint8_t cur[6] = {0x1E, 0x1F, 0, 0, 0, 0};
    uint8_t out[6] = {0};
    size_t n = hid_keymap_new_keys(prev, cur, out);
    CHECK(n == 1);
    CHECK(out[0] == 0x1F);
}

static void test_new_keys_same_report_twice_is_empty(void)
{
    const uint8_t rep[6] = {0x1E, 0, 0, 0, 0, 0};
    uint8_t out[6] = {0};
    CHECK(hid_keymap_new_keys(rep, rep, out) == 0);
}

static void test_new_keys_ignores_rollover(void)
{
    const uint8_t prev[6] = {0};
    const uint8_t cur[6] = {0x01, 0x01, 0x01, 0x01, 0x01, 0x01};
    uint8_t out[6] = {0};
    CHECK(hid_keymap_new_keys(prev, cur, out) == 0);
}

int main(void)
{
    RUN(test_digits_and_letters);
    RUN(test_shift_symbols);
    RUN(test_control_keys);
    RUN(test_keypad_digits);
    RUN(test_unsupported_returns_zero);
    RUN(test_shift_modifier_bits);
    RUN(test_new_keys_ignores_held_keys);
    RUN(test_new_keys_same_report_twice_is_empty);
    RUN(test_new_keys_ignores_rollover);
    return TEST_SUMMARY();
}
