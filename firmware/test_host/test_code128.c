#include "code128.h"
#include "test_util.h"

#include <stdlib.h>
#include <string.h>

/* Jeff 2026-11-23 提供的 App 截圖（領球碼 4346）量到的黑白段寬度，單位是像素。黑段開始。 */
static const int APP_RUNS[] = {
    20, 12, 9, 23, 8, 43, 9, 31, 12, 20, 20, 20, 23, 20, 9, 23, 31, 9, 20, 23, 9, 11, 32, 20, 20,
    20, 12, 20, 31, 12, 20, 20, 32, 8, 12, 20, 43, 20, 11, 9, 12, 20, 20, 31, 32, 11, 9, 12, 20,
};
#define APP_RUN_COUNT ((int)(sizeof APP_RUNS / sizeof APP_RUNS[0]))

#define ROW_MAX 1400
#define WHITE   230
#define BLACK   25

/* 把黑白段畫成一列像素：左邊留 quiet 像素的空白，每一段寬度乘 scale。回傳像素數。 */
static int draw(const int *runs, int count, float scale, int quiet, uint8_t *row)
{
    int n = 0;
    for (int i = 0; i < quiet; i++) row[n++] = WHITE;
    float edge = (float)quiet;
    for (int r = 0; r < count; r++) {
        edge += (float)runs[r] * scale;
        while (n < (int)(edge + 0.5f) && n < ROW_MAX) row[n++] = r % 2 == 0 ? BLACK : WHITE;
    }
    for (int i = 0; i < quiet && n < ROW_MAX; i++) row[n++] = WHITE;
    return n;
}

/* 左右各取一點平均：模擬鏡頭的模糊 */
static void blur(uint8_t *row, int n)
{
    uint8_t tmp[ROW_MAX];
    memcpy(tmp, row, (size_t)n);
    for (int i = 1; i + 1 < n; i++) {
        row[i] = (uint8_t)((tmp[i - 1] + 2 * tmp[i] + tmp[i + 1]) / 4);
    }
}

static void test_app_barcode_decodes_to_percent_and_code(void)
{
    uint8_t row[ROW_MAX];
    char out[CODE128_TEXT_MAX];
    const int n = draw(APP_RUNS, APP_RUN_COUNT, 1.0f, 60, row);
    CHECK(code128_decode_row(row, n, out, sizeof out) == 5);
    CHECK_STR(out, "%4346");
}

static void test_small_blurred_barcode_still_decodes(void)
{
    /* 鏡頭裡的條碼：每個單位大約 4 個像素，而且有一點糊 */
    uint8_t row[ROW_MAX];
    char out[CODE128_TEXT_MAX];
    const int n = draw(APP_RUNS, APP_RUN_COUNT, 0.4f, 40, row);
    blur(row, n);
    CHECK(code128_decode_row(row, n, out, sizeof out) == 5);
    CHECK_STR(out, "%4346");
}

static void test_reversed_barcode_decodes(void)
{
    uint8_t row[ROW_MAX];
    uint8_t rev[ROW_MAX];
    char out[CODE128_TEXT_MAX];
    const int n = draw(APP_RUNS, APP_RUN_COUNT, 0.5f, 40, row);
    for (int i = 0; i < n; i++) rev[i] = row[n - 1 - i];
    CHECK(code128_decode_row(rev, n, out, sizeof out) == 5);
    CHECK_STR(out, "%4346");
}

static void test_uneven_lighting_is_tolerated(void)
{
    /* 左邊亮、右邊暗（手機螢幕反光） */
    uint8_t row[ROW_MAX];
    char out[CODE128_TEXT_MAX];
    const int n = draw(APP_RUNS, APP_RUN_COUNT, 0.5f, 40, row);
    for (int i = 0; i < n; i++) {
        row[i] = (uint8_t)(row[i] * (100 - 45 * i / n) / 100);
    }
    CHECK(code128_decode_row(row, n, out, sizeof out) == 5);
    CHECK_STR(out, "%4346");
}

/* 黑線每一條變細 shrink 個像素，白線等量變寬：模擬手機螢幕太亮時白色溢到黑線上 */
static int draw_bloomed(float scale, float shrink, int quiet, uint8_t *row)
{
    int n = 0;
    for (int i = 0; i < quiet; i++) row[n++] = WHITE;
    float edge = (float)quiet;
    for (int r = 0; r < APP_RUN_COUNT; r++) {
        const float next = edge + (float)APP_RUNS[r] * scale;
        /* 黑段：左右各縮 shrink/2 */
        const float from = r % 2 == 0 ? edge + shrink / 2 : edge - shrink / 2;
        const float to = r % 2 == 0 ? next - shrink / 2 : next + shrink / 2;
        (void)from;
        while (n < (int)(to + 0.5f) && n < ROW_MAX) row[n++] = r % 2 == 0 ? BLACK : WHITE;
        edge = next;
    }
    for (int i = 0; i < quiet && n < ROW_MAX; i++) row[n++] = WHITE;
    return n;
}

static void test_thin_bars_from_bright_screen_still_decode(void)
{
    /* 2026-11-23 實拍：手機螢幕很亮，細的黑線變得更細。每單位 5 像素，黑線各少 2 像素 */
    uint8_t row[ROW_MAX];
    char out[CODE128_TEXT_MAX];
    const int n = draw_bloomed(0.5f, 2.0f, 40, row);
    CHECK(code128_decode_row(row, n, out, sizeof out) == 5);
    CHECK_STR(out, "%4346");
}

static void test_damaged_barcode_is_rejected(void)
{
    /* 把一個資料符號換成別的合法符號：寬度比例都對，但檢查碼不對，必須拒絕 */
    int runs[APP_RUN_COUNT];
    memcpy(runs, APP_RUNS, sizeof runs);
    /* 第 3 個符號（位置 12..17）原本是 '4'（221231），改成 '3'（221132） */
    runs[15] = 9; runs[16] = 31; runs[17] = 20;
    uint8_t row[ROW_MAX];
    char out[CODE128_TEXT_MAX] = "x";
    const int n = draw(runs, APP_RUN_COUNT, 1.0f, 60, row);
    CHECK(code128_decode_row(row, n, out, sizeof out) == -1);
    CHECK_STR(out, "");
}

static void test_truncated_barcode_is_rejected(void)
{
    /* 只拍到前半段：沒有結束碼 */
    uint8_t row[ROW_MAX];
    char out[CODE128_TEXT_MAX];
    const int n = draw(APP_RUNS, 30, 1.0f, 60, row);
    CHECK(code128_decode_row(row, n, out, sizeof out) == -1);
}

/* 條碼右邊超出畫面：畫出前 count 段之後直接到影像邊緣，沒有留白 */
static int draw_cut(int count, float scale, int quiet, uint8_t *row)
{
    int n = 0;
    for (int i = 0; i < quiet; i++) row[n++] = WHITE;
    float edge = (float)quiet;
    for (int r = 0; r < count; r++) {
        edge += (float)APP_RUNS[r] * scale;
        while (n < (int)(edge + 0.5f) && n < ROW_MAX) row[n++] = r % 2 == 0 ? BLACK : WHITE;
    }
    return n;
}

static void test_scan_reports_complete_barcode(void)
{
    uint8_t row[ROW_MAX];
    char out[CODE128_TEXT_MAX];
    const int n = draw(APP_RUNS, APP_RUN_COUNT, 0.5f, 40, row);
    CHECK(code128_scan_row(row, n, out, sizeof out) == CODE128_OK);
    CHECK_STR(out, "%4346");
}

static void test_scan_reports_partial_when_end_is_cut_off(void)
{
    /* 2026-11-23 實拍：手機太近，條碼右半邊超出畫面。要能提示「請拿遠一點」 */
    uint8_t row[ROW_MAX];
    char out[CODE128_TEXT_MAX] = "x";
    const int n = draw_cut(27, 0.8f, 70, row);  /* 起始碼＋3 個符號＋半個 */
    CHECK(code128_scan_row(row, n, out, sizeof out) == CODE128_PARTIAL);
    CHECK_STR(out, "");
}

static void test_scan_reports_partial_when_start_is_cut_off(void)
{
    /* 左半邊超出畫面：把上一個影像左右對調 */
    uint8_t row[ROW_MAX];
    uint8_t rev[ROW_MAX];
    char out[CODE128_TEXT_MAX];
    const int n = draw_cut(27, 0.8f, 70, row);
    for (int i = 0; i < n; i++) rev[i] = row[n - 1 - i];
    CHECK(code128_scan_row(rev, n, out, sizeof out) == CODE128_PARTIAL);
}

static void test_scan_reports_partial_when_only_the_end_is_visible(void)
{
    /* 條碼的左半邊超出畫面，只看得到後半段與結束碼（影像左緣直接切在條碼中間） */
    uint8_t row[ROW_MAX];
    char out[CODE128_TEXT_MAX];
    int n = 0;
    float edge = 0;
    for (int r = 20; r < APP_RUN_COUNT; r++) {
        edge += (float)APP_RUNS[r] * 0.8f;
        while (n < (int)(edge + 0.5f) && n < ROW_MAX) row[n++] = r % 2 == 0 ? BLACK : WHITE;
    }
    for (int i = 0; i < 60; i++) row[n++] = WHITE;
    CHECK(code128_scan_row(row, n, out, sizeof out) == CODE128_PARTIAL);
}

static void test_scan_is_not_partial_for_a_start_code_alone(void)
{
    /* 只有起始碼加一個符號：證據不夠，不提示 */
    uint8_t row[ROW_MAX];
    char out[CODE128_TEXT_MAX];
    const int n = draw_cut(12, 0.8f, 70, row);
    CHECK(code128_scan_row(row, n, out, sizeof out) == CODE128_NONE);
}

static void test_scan_is_not_partial_for_a_damaged_complete_barcode(void)
{
    /* 整條都在畫面裡但檢查碼錯：不是「太近」 */
    int runs[APP_RUN_COUNT];
    memcpy(runs, APP_RUNS, sizeof runs);
    runs[15] = 9; runs[16] = 31; runs[17] = 20;
    uint8_t row[ROW_MAX];
    char out[CODE128_TEXT_MAX];
    const int n = draw(runs, APP_RUN_COUNT, 1.0f, 60, row);
    CHECK(code128_scan_row(row, n, out, sizeof out) == CODE128_NONE);
}

static void test_noise_and_blank_rows_are_rejected(void)
{
    uint8_t row[ROW_MAX];
    char out[CODE128_TEXT_MAX];
    memset(row, WHITE, sizeof row);
    CHECK(code128_decode_row(row, ROW_MAX, out, sizeof out) == -1);
    srand(12345);
    int false_reads = 0;
    int false_partials = 0;
    for (int round = 0; round < 2000; round++) {
        /* 隨機寬度的黑白段：看起來像條碼，但不是 */
        int n = 0;
        int black = 0;
        while (n < ROW_MAX) {
            const int width = 3 + rand() % 14;
            for (int i = 0; i < width && n < ROW_MAX; i++) row[n++] = black ? BLACK : WHITE;
            black = !black;
        }
        if (code128_decode_row(row, ROW_MAX, out, sizeof out) > 0) false_reads++;
        if (code128_scan_row(row, ROW_MAX, out, sizeof out) == CODE128_PARTIAL) false_partials++;
    }
    CHECK(false_reads == 0);
    CHECK(false_partials <= 2);  /* 提示文字偶爾誤報可以接受，但不得常常出現 */
}

static void test_bad_arguments(void)
{
    uint8_t row[16] = {0};
    char out[CODE128_TEXT_MAX];
    CHECK(code128_decode_row(NULL, 16, out, sizeof out) == -1);
    CHECK(code128_decode_row(row, 16, NULL, 0) == -1);
    CHECK(code128_decode_row(row, 0, out, sizeof out) == -1);
    CHECK(code128_decode_row(row, 16, out, sizeof out) == -1);
}

static void test_output_too_small_is_rejected(void)
{
    uint8_t row[ROW_MAX];
    char out[4];
    const int n = draw(APP_RUNS, APP_RUN_COUNT, 1.0f, 60, row);
    CHECK(code128_decode_row(row, n, out, sizeof out) == -1);
}

int main(void)
{
    RUN(test_app_barcode_decodes_to_percent_and_code);
    RUN(test_small_blurred_barcode_still_decodes);
    RUN(test_reversed_barcode_decodes);
    RUN(test_uneven_lighting_is_tolerated);
    RUN(test_thin_bars_from_bright_screen_still_decode);
    RUN(test_damaged_barcode_is_rejected);
    RUN(test_truncated_barcode_is_rejected);
    RUN(test_scan_reports_complete_barcode);
    RUN(test_scan_reports_partial_when_end_is_cut_off);
    RUN(test_scan_reports_partial_when_start_is_cut_off);
    RUN(test_scan_reports_partial_when_only_the_end_is_visible);
    RUN(test_scan_is_not_partial_for_a_start_code_alone);
    RUN(test_scan_is_not_partial_for_a_damaged_complete_barcode);
    RUN(test_noise_and_blank_rows_are_rejected);
    RUN(test_bad_arguments);
    RUN(test_output_too_small_is_rejected);
    return TEST_SUMMARY();
}
