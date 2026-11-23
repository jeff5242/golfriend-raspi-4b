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

static void test_gray_half_uses_green_channel(void)
{
    /* 4x2 影像：左邊 2x2 全白，右邊 2x2 全紅（沒有綠色） */
    const uint8_t src[] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xF8, 0x00, 0xF8,
        0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xF8, 0x00, 0xF8,
    };
    uint8_t dst[2] = {1, 1};
    qr_gray_half_from_rgb565(src, 4, 2, dst, false);
    CHECK(dst[0] == 0xFC);
    CHECK(dst[1] == 0x00);
}

static void test_gray_half_can_mirror_each_row(void)
{
    /* 感光元件輸出的影像左右相反；辨識前要翻回來。4x2 影像：左邊 2x2 全黑，右邊 2x2 全綠 */
    const uint8_t src[] = {
        0x00, 0x00, 0x00, 0x00, 0xE0, 0x07, 0xE0, 0x07,
        0x00, 0x00, 0x00, 0x00, 0xE0, 0x07, 0xE0, 0x07,
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
    CHECK(qr_status_update(&st, QR_SCAN_IDLE, 0, 1500) == QR_SCAN_IDLE);
    CHECK(qr_status_update(&st, QR_SCAN_FOUND, 100, 1500) == QR_SCAN_FOUND);
    CHECK(qr_status_update(&st, QR_SCAN_READ, 200, 1500) == QR_SCAN_READ);
}

static void test_status_holds_before_going_back_to_idle(void)
{
    /* 辨識結果一張有、一張沒有時，畫面上的字不得閃爍 */
    qr_status_t st = {0};
    CHECK(qr_status_update(&st, QR_SCAN_FOUND, 1000, 1500) == QR_SCAN_FOUND);
    CHECK(qr_status_update(&st, QR_SCAN_IDLE, 1100, 1500) == QR_SCAN_FOUND);
    CHECK(qr_status_update(&st, QR_SCAN_IDLE, 2500, 1500) == QR_SCAN_FOUND);
    CHECK(qr_status_update(&st, QR_SCAN_IDLE, 2501, 1500) == QR_SCAN_IDLE);
}

static void test_status_read_is_held_over_found(void)
{
    qr_status_t st = {0};
    CHECK(qr_status_update(&st, QR_SCAN_READ, 1000, 1500) == QR_SCAN_READ);
    CHECK(qr_status_update(&st, QR_SCAN_FOUND, 1200, 1500) == QR_SCAN_READ);
    CHECK(qr_status_update(&st, QR_SCAN_FOUND, 2600, 1500) == QR_SCAN_FOUND);
    CHECK(qr_status_update(NULL, QR_SCAN_READ, 0, 1500) == QR_SCAN_IDLE);
}

static void test_status_too_close_is_shown_and_held(void)
{
    /* 只拍到半截條碼：提示「請拿遠一點」，而且不被「偵測到」蓋掉 */
    qr_status_t st = {0};
    CHECK(qr_status_update(&st, QR_SCAN_TOO_CLOSE, 1000, 1500) == QR_SCAN_TOO_CLOSE);
    CHECK(qr_status_update(&st, QR_SCAN_FOUND, 1200, 1500) == QR_SCAN_TOO_CLOSE);
    CHECK(qr_status_update(&st, QR_SCAN_IDLE, 2400, 1500) == QR_SCAN_TOO_CLOSE);
    CHECK(qr_status_update(&st, QR_SCAN_READ, 2450, 1500) == QR_SCAN_READ);
}

static void test_gray_crop_copies_the_centre_at_full_resolution(void)
{
    /* 遠一點的 QR Code 在影像裡比較小：中間那一塊不縮小，直接取用 */
    uint8_t src[6 * 4 * 2] = {0};
    /* 6x4 影像，只有 (2,1) 與 (3,2) 是綠色 */
    src[(1 * 6 + 2) * 2] = 0xE0; src[(1 * 6 + 2) * 2 + 1] = 0x07;
    src[(2 * 6 + 3) * 2] = 0xE0; src[(2 * 6 + 3) * 2 + 1] = 0x07;
    uint8_t dst[4] = {9, 9, 9, 9};
    qr_gray_crop_from_rgb565(src, 6, 4, 2, 2, dst, false);  /* 中間 2x2：x 2..3，y 1..2 */
    CHECK(dst[0] == 0xFC && dst[1] == 0x00 && dst[2] == 0x00 && dst[3] == 0xFC);
    qr_gray_crop_from_rgb565(src, 6, 4, 2, 2, dst, true);
    CHECK(dst[0] == 0x00 && dst[1] == 0xFC && dst[2] == 0xFC && dst[3] == 0x00);
}

static void test_gray_half_averages_each_2x2_block(void)
{
    /* 手機螢幕有細紋；只取單一像素會出現雜訊，所以每 2x2 取平均 */
    const uint8_t src[] = {
        0xE0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* 綠 黑 | 黑 黑 */
        0xE0, 0x07, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x07,   /* 綠 黑 | 黑 綠 */
    };
    uint8_t dst[2] = {1, 1};
    qr_gray_half_from_rgb565(src, 4, 2, dst, false);
    CHECK(dst[0] == 0x7E);  /* (0xFC + 0 + 0xFC + 0) / 4 */
    CHECK(dst[1] == 0x3F);  /* (0 + 0 + 0 + 0xFC) / 4 */
}

int main(void)
{
    RUN(test_plain_digits_pass_through);
    RUN(test_non_digit_characters_are_removed);
    RUN(test_payload_without_usable_digits_is_rejected);
    RUN(test_gray_half_uses_green_channel);
    RUN(test_gray_half_averages_each_2x2_block);
    RUN(test_gray_half_can_mirror_each_row);
    RUN(test_status_shows_found_then_read);
    RUN(test_status_holds_before_going_back_to_idle);
    RUN(test_status_read_is_held_over_found);
    RUN(test_status_too_close_is_shown_and_held);
    RUN(test_gray_crop_copies_the_centre_at_full_resolution);
    return TEST_SUMMARY();
}
