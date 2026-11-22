#include "app_typed.h"
#include "test_util.h"

static void test_digits_append(void)
{
    char typed[8] = "";
    CHECK(!app_typed_apply(typed, sizeof typed, '1'));
    CHECK(!app_typed_apply(typed, sizeof typed, '2'));
    CHECK_STR(typed, "12");
}

static void test_backspace_deletes_last_char(void)
{
    char typed[8] = "12";
    CHECK(!app_typed_apply(typed, sizeof typed, '\b'));
    CHECK_STR(typed, "1");
    CHECK(!app_typed_apply(typed, sizeof typed, '\b'));
    CHECK(!app_typed_apply(typed, sizeof typed, '\b'));  /* 空的時候不出錯 */
    CHECK_STR(typed, "");
}

static void test_enter_submits_only_when_not_empty(void)
{
    char typed[8] = "";
    CHECK(!app_typed_apply(typed, sizeof typed, '\n'));
    CHECK(!app_typed_apply(typed, sizeof typed, '7'));
    CHECK(app_typed_apply(typed, sizeof typed, '\n'));
    CHECK_STR(typed, "7");  /* 送出時內容不變，由呼叫端取用 */
}

static void test_full_buffer_drops_extra_chars(void)
{
    char typed[4] = "";
    for (int i = 0; i < 6; i++) {
        CHECK(!app_typed_apply(typed, sizeof typed, '9'));
    }
    CHECK_STR(typed, "999");
}

static void test_control_chars_ignored(void)
{
    char typed[8] = "1";
    CHECK(!app_typed_apply(typed, sizeof typed, '\t'));
    CHECK(!app_typed_apply(typed, sizeof typed, 0x7F));
    CHECK_STR(typed, "1");
    CHECK(!app_typed_apply(NULL, 8, '1'));
    CHECK(!app_typed_apply(typed, 0, '1'));
}

int main(void)
{
    RUN(test_digits_append);
    RUN(test_backspace_deletes_last_char);
    RUN(test_enter_submits_only_when_not_empty);
    RUN(test_full_buffer_drops_extra_chars);
    RUN(test_control_chars_ignored);
    return TEST_SUMMARY();
}
