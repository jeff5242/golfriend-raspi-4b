#include "qr_code.h"
#include "test_util.h"

#include <string.h>

static bool conv(const char *payload, char *out, size_t size)
{
    return qr_code_from_payload((const uint8_t *)payload, strlen(payload), out, size);
}

static void test_plain_digits_pass_through(void)
{
    char out[QR_CODE_MAX];
    CHECK(conv("4175", out, sizeof out));
    CHECK_STR(out, "4175");
    CHECK(conv("0899", out, sizeof out));
    CHECK_STR(out, "0899");  /* 開頭的 0 必須保留 */
}

static void test_non_digit_characters_are_removed(void)
{
    /* Jeff 2026-11-23：QR 內容是 "%2811"，非數字的字元都拿掉 */
    char out[QR_CODE_MAX];
    CHECK(conv("%2811", out, sizeof out));
    CHECK_STR(out, "2811");
    CHECK(conv("%8200%\r\n", out, sizeof out));
    CHECK_STR(out, "8200");
    CHECK(conv("12 34", out, sizeof out));
    CHECK_STR(out, "1234");
    CHECK(conv("A1-2\x01" "3\xe9\xa0\x98" "4", out, sizeof out));
    CHECK_STR(out, "1234");
}

static void test_payload_without_usable_digits_is_rejected(void)
{
    char out[QR_CODE_MAX];
    CHECK(!conv("", out, sizeof out));
    CHECK(!conv("%%", out, sizeof out));
    CHECK(!conv("https://example.com/", out, sizeof out));
    CHECK(!conv("0123456789012345678901234567890123456789", out, sizeof out)); /* 太長 */
    CHECK(!qr_code_from_payload(NULL, 4, out, sizeof out));
    CHECK(!conv("1234", NULL, 0));
    char small[4];
    CHECK(!conv("1234", small, sizeof small));
}

static void test_gate_sends_a_code_once_while_it_stays_in_view(void)
{
    qr_gate_t g = {0};
    CHECK(qr_gate_accept(&g, "4175", 1000, 5000));
    CHECK(!qr_gate_accept(&g, "4175", 1200, 5000));
    CHECK(!qr_gate_accept(&g, "4175", 6000, 5000));   /* 一直在鏡頭前：每次看到都延長 */
    CHECK(!qr_gate_accept(&g, "4175", 10900, 5000));
}

static void test_gate_allows_same_code_after_it_left_long_enough(void)
{
    qr_gate_t g = {0};
    CHECK(qr_gate_accept(&g, "4175", 1000, 5000));
    CHECK(qr_gate_accept(&g, "4175", 6001, 5000));
}

static void test_gate_accepts_a_different_code_immediately(void)
{
    qr_gate_t g = {0};
    CHECK(qr_gate_accept(&g, "4175", 1000, 5000));
    CHECK(qr_gate_accept(&g, "8200", 1100, 5000));
    CHECK(!qr_gate_accept(&g, "8200", 1200, 5000));
}

static void test_gate_survives_tick_wraparound(void)
{
    qr_gate_t g = {0};
    CHECK(qr_gate_accept(&g, "4175", 0xFFFFFF00u, 5000));
    CHECK(!qr_gate_accept(&g, "4175", 0x00000100u, 5000));  /* 過了 512 ms */
    CHECK(qr_gate_accept(&g, "4175", 0x00000100u + 6000, 5000));
}

static void test_gate_rejects_null_and_empty(void)
{
    qr_gate_t g = {0};
    CHECK(!qr_gate_accept(NULL, "1", 0, 5000));
    CHECK(!qr_gate_accept(&g, NULL, 0, 5000));
    CHECK(!qr_gate_accept(&g, "", 0, 5000));
}

static void test_gray_half_uses_green_channel_of_every_other_pixel(void)
{
    /* 4x2 影像：白、黑、純綠、純紅／第二列不取 */
    const uint8_t src[] = {
        0xFF, 0xFF, 0x00, 0x00, 0xE0, 0x07, 0x00, 0xF8,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    uint8_t dst[2] = {1, 1};
    qr_gray_half_from_rgb565(src, 4, 2, dst, false);
    CHECK(dst[0] == 0xFC);  /* 第 0 個像素：白，綠色 6 位元全滿 */
    CHECK(dst[1] == 0xFC);  /* 第 2 個像素：純綠 */
}

static void test_gray_half_can_mirror_each_row(void)
{
    /* 感光元件輸出的影像左右相反；辨識前要翻回來。4x2 影像：黑、黑、純綠、黑 */
    const uint8_t src[] = {
        0x00, 0x00, 0x00, 0x00, 0xE0, 0x07, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    uint8_t dst[2] = {1, 1};
    qr_gray_half_from_rgb565(src, 4, 2, dst, false);
    CHECK(dst[0] == 0x00 && dst[1] == 0xFC);
    qr_gray_half_from_rgb565(src, 4, 2, dst, true);
    CHECK(dst[0] == 0xFC && dst[1] == 0x00);
}

static void test_status_shows_found_then_read(void)
{
    qr_status_t st = {0};
    CHECK(qr_status_update(&st, false, false, 0, 1500) == QR_SCAN_IDLE);
    CHECK(qr_status_update(&st, true, false, 100, 1500) == QR_SCAN_FOUND);
    CHECK(qr_status_update(&st, true, true, 200, 1500) == QR_SCAN_READ);
}

static void test_status_holds_before_going_back_to_idle(void)
{
    /* 辨識結果一張有、一張沒有時，畫面上的字不得閃爍 */
    qr_status_t st = {0};
    CHECK(qr_status_update(&st, true, false, 1000, 1500) == QR_SCAN_FOUND);
    CHECK(qr_status_update(&st, false, false, 1100, 1500) == QR_SCAN_FOUND);
    CHECK(qr_status_update(&st, false, false, 2500, 1500) == QR_SCAN_FOUND);
    CHECK(qr_status_update(&st, false, false, 2501, 1500) == QR_SCAN_IDLE);
}

static void test_status_read_is_not_replaced_by_found_during_hold(void)
{
    /* 讀到之後，同一張碼還在鏡頭前：維持「已讀取」，不退回「偵測到」 */
    qr_status_t st = {0};
    CHECK(qr_status_update(&st, true, true, 1000, 1500) == QR_SCAN_READ);
    CHECK(qr_status_update(&st, true, false, 1200, 1500) == QR_SCAN_READ);
    CHECK(qr_status_update(&st, true, false, 2600, 1500) == QR_SCAN_FOUND);
    CHECK(qr_status_update(NULL, true, true, 0, 1500) == QR_SCAN_IDLE);
}

int main(void)
{
    RUN(test_plain_digits_pass_through);
    RUN(test_non_digit_characters_are_removed);
    RUN(test_payload_without_usable_digits_is_rejected);
    RUN(test_gate_sends_a_code_once_while_it_stays_in_view);
    RUN(test_gate_allows_same_code_after_it_left_long_enough);
    RUN(test_gate_accepts_a_different_code_immediately);
    RUN(test_gate_survives_tick_wraparound);
    RUN(test_gate_rejects_null_and_empty);
    RUN(test_gray_half_uses_green_channel_of_every_other_pixel);
    RUN(test_gray_half_can_mirror_each_row);
    RUN(test_status_shows_found_then_read);
    RUN(test_status_holds_before_going_back_to_idle);
    RUN(test_status_read_is_not_replaced_by_found_during_hold);
    return TEST_SUMMARY();
}
