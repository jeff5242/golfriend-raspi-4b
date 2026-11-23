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
}

static void test_scanner_affixes_and_whitespace_are_removed(void)
{
    char out[QR_CODE_MAX];
    CHECK(conv("%8200%", out, sizeof out));
    CHECK_STR(out, "8200");
    CHECK(conv("  0899\r\n", out, sizeof out));
    CHECK_STR(out, "0899");  /* 開頭的 0 必須保留 */
    CHECK(conv("%1234\t", out, sizeof out));
    CHECK_STR(out, "1234");
}

static void test_bad_payloads_are_rejected(void)
{
    char out[QR_CODE_MAX];
    CHECK(!conv("", out, sizeof out));
    CHECK(!conv("%%", out, sizeof out));
    CHECK(!conv(" \n", out, sizeof out));
    CHECK(!conv("12\x01" "34", out, sizeof out));          /* 控制字元 */
    CHECK(!conv("12 34", out, sizeof out));                /* 中間有空白 */
    CHECK(!conv("\xe9\xa0\x98\xe7\x90\x83", out, sizeof out)); /* 非 ASCII */
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
    qr_gray_half_from_rgb565(src, 4, 2, dst);
    CHECK(dst[0] == 0xFC);  /* 第 0 個像素：白，綠色 6 位元全滿 */
    CHECK(dst[1] == 0xFC);  /* 第 2 個像素：純綠 */
}

int main(void)
{
    RUN(test_plain_digits_pass_through);
    RUN(test_scanner_affixes_and_whitespace_are_removed);
    RUN(test_bad_payloads_are_rejected);
    RUN(test_gate_sends_a_code_once_while_it_stays_in_view);
    RUN(test_gate_allows_same_code_after_it_left_long_enough);
    RUN(test_gate_accepts_a_different_code_immediately);
    RUN(test_gate_survives_tick_wraparound);
    RUN(test_gate_rejects_null_and_empty);
    RUN(test_gray_half_uses_green_channel_of_every_other_pixel);
    return TEST_SUMMARY();
}
