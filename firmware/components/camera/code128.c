#include "code128.h"

#include <stdbool.h>
#include <string.h>

/*
 * 符號表：每個符號 6 段（黑白交替，黑先），寬度合計 11 個單位；結束碼 7 段、13 個單位。
 * 來源：ZXing Code128Reader.CODE_PATTERNS（Apache-2.0），即 ISO/IEC 15417 的表。
 */
static const char *const PATTERNS[107] = {
    "212222", "222122", "222221", "121223", "121322", "131222",
    "122213", "122312", "132212", "221213", "221312", "231212",
    "112232", "122132", "122231", "113222", "123122", "123221",
    "223211", "221132", "221231", "213212", "223112", "312131",
    "311222", "321122", "321221", "312212", "322112", "322211",
    "212123", "212321", "232121", "111323", "131123", "131321",
    "112313", "132113", "132311", "211313", "231113", "231311",
    "112133", "112331", "132131", "113123", "113321", "133121",
    "313121", "211331", "231131", "213113", "213311", "213131",
    "311123", "311321", "331121", "312113", "312311", "332111",
    "314111", "221411", "431111", "111224", "111422", "121124",
    "121421", "141122", "141221", "112214", "112412", "122114",
    "122411", "142112", "142211", "241211", "221114", "413111",
    "241112", "134111", "111242", "121142", "121241", "114212",
    "124112", "124211", "411212", "421112", "421211", "212141",
    "214121", "412121", "111143", "111341", "131141", "114113",
    "114311", "411113", "411311", "113141", "114131", "311141",
    "411131", "211412", "211214", "211232", "2331112",
};

#define SYMBOL_RUNS      6
#define SYMBOL_UNITS     11
#define STOP_RUNS        7
#define CODE_SHIFT       98
#define CODE_C           99
#define CODE_B           100
#define CODE_A           101
#define START_A          103
#define START_B          104
#define START_C          105
#define CODE_STOP        106
#define CHECK_MODULUS    103
#define MAX_RUNS         512
#define MAX_SYMBOLS      48
#define BLOCK            32    /* 算區域亮度用的區塊寬度（像素） */
#define MIN_CONTRAST     48    /* 區域最亮與最暗差不到這個值：當成沒有條碼 */
#define MAX_AVG_VARIANCE 0.25f /* 同 ZXing：整個符號的平均誤差上限（單位寬度的比例） */
#define MAX_ONE_VARIANCE 0.7f  /* 同 ZXing：單一段的誤差上限 */

/* 把一列像素轉成黑白段的寬度。runs[0] 是白段（可能是 0）。回傳段數。 */
static int to_runs(const uint8_t *gray, int n, uint16_t *runs, int max_runs)
{
    /* 每個區塊的最亮與最暗值；門檻取相鄰三個區塊的中間值，抵抗亮度不均 */
    uint8_t lo[1 + 4096 / BLOCK];
    uint8_t hi[1 + 4096 / BLOCK];
    const int blocks = (n + BLOCK - 1) / BLOCK;
    if (n <= 0 || blocks > (int)sizeof lo) {
        return 0;
    }
    for (int b = 0; b < blocks; b++) {
        uint8_t mn = 255;
        uint8_t mx = 0;
        const int end = (b + 1) * BLOCK < n ? (b + 1) * BLOCK : n;
        for (int i = b * BLOCK; i < end; i++) {
            if (gray[i] < mn) mn = gray[i];
            if (gray[i] > mx) mx = gray[i];
        }
        lo[b] = mn;
        hi[b] = mx;
    }
    int count = 0;
    bool black = false;
    int len = 0;
    for (int i = 0; i < n; i++) {
        const int b = i / BLOCK;
        uint8_t mn = lo[b];
        uint8_t mx = hi[b];
        if (b > 0) {
            if (lo[b - 1] < mn) mn = lo[b - 1];
            if (hi[b - 1] > mx) mx = hi[b - 1];
        }
        if (b + 1 < blocks) {
            if (lo[b + 1] < mn) mn = lo[b + 1];
            if (hi[b + 1] > mx) mx = hi[b + 1];
        }
        const bool is_black = mx - mn >= MIN_CONTRAST && gray[i] < (uint8_t)((mn + mx) / 2);
        if (is_black == black) {
            len++;
            continue;
        }
        if (count >= max_runs) {
            return count;
        }
        runs[count++] = (uint16_t)len;
        black = is_black;
        len = 1;
    }
    if (count < max_runs) {
        runs[count++] = (uint16_t)len;
    }
    return count;
}

/*
 * 這幾段和符號 pattern 的差距（越小越像）。不可能是這個符號時回傳很大的值。
 *
 * 比的是「相鄰兩段的寬度和」（黑線左緣到下一條黑線左緣、白線左緣到下一條白線左緣）。
 * 手機螢幕很亮時，黑線會整條變細、白線等量變寬；相鄰兩段的和不受影響。
 * Code 128 的每個符號黑線總寬都是偶數，所以這幾個和就能唯一決定一個符號。
 */
static float variance(const uint16_t *runs, const char *pattern, int run_count, int units)
{
    int total = 0;
    for (int i = 0; i < run_count; i++) {
        total += runs[i];
    }
    if (total < units) {
        return 1e9f;
    }
    const float unit = (float)total / (float)units;
    const float max_one = MAX_ONE_VARIANCE * unit;
    float sum = 0;
    for (int i = 0; i + 1 < run_count; i++) {
        const float want = (float)((pattern[i] - '0') + (pattern[i + 1] - '0')) * unit;
        const float got = (float)(runs[i] + runs[i + 1]);
        const float diff = got > want ? got - want : want - got;
        if (diff > max_one) {
            return 1e9f;
        }
        sum += diff;
    }
    /* 相鄰和有 run_count - 1 個；換算成和原本「每段誤差／總寬」相同的尺度 */
    return sum / (float)total;
}

/* 找最像的符號（0..105）。沒有夠像的回傳 -1。 */
static int best_symbol(const uint16_t *runs, int first, int last)
{
    float best = MAX_AVG_VARIANCE;
    int found = -1;
    for (int v = first; v <= last; v++) {
        const float var = variance(runs, PATTERNS[v], SYMBOL_RUNS, SYMBOL_UNITS);
        if (var < best) {
            best = var;
            found = v;
        }
    }
    return found;
}

/* 把符號值轉成文字。values 不含起始碼、檢查碼。回傳字元數，內容不合法回傳 -1。 */
static int to_text(int start, const int *values, int count, char *out, size_t out_size)
{
    int set = start == START_A ? 'A' : start == START_B ? 'B' : 'C';
    int shift_from = 0;  /* 非 0：上一個是 SHIFT，這個符號用另一組，之後換回 shift_from */
    size_t n = 0;
    for (int i = 0; i < count; i++) {
        const int v = values[i];
        char buf[3] = "";
        if (set == 'C') {
            if (v < 100) {
                buf[0] = (char)('0' + v / 10);
                buf[1] = (char)('0' + v % 10);
            } else if (v == CODE_B) {
                set = 'B';
            } else if (v == CODE_A) {
                set = 'A';
            }
        } else if (v < 96) {
            /* B 組：值 0..95 = ASCII 32..127。A 組：0..63 = ASCII 32..95，64..95 = 控制字元 */
            buf[0] = set == 'B' || v < 64 ? (char)(v + 32) : (char)(v - 64);
        } else if (v == CODE_SHIFT) {
            shift_from = set;
            set = set == 'A' ? 'B' : 'A';
            continue;
        } else if (v == CODE_C) {
            set = 'C';
        } else if (v == CODE_B && set == 'A') {
            set = 'B';
        } else if (v == CODE_A && set == 'B') {
            set = 'A';
        }
        for (const char *c = buf; *c != '\0'; c++) {
            if (n + 1 >= out_size) {
                return -1;
            }
            out[n++] = *c;
        }
        if (shift_from != 0) {
            set = shift_from;
            shift_from = 0;
        }
    }
    out[n] = '\0';
    return (int)n;
}

#define PARTIAL_MIN_SYMBOLS 3  /* 起始碼（或結束碼）之外至少看到幾個符號，才說是「半截條碼」 */
#define RESULT_NONE    (-1)
#define RESULT_PARTIAL (-2)

static int group_width(const uint16_t *runs)
{
    int width = 0;
    for (int i = 0; i < SYMBOL_RUNS; i++) {
        width += runs[i];
    }
    return width;
}

/* 同一個條碼裡每個符號一樣寬（11 個單位）。差太多就不是同一個條碼 */
static bool width_matches(int width, int reference)
{
    return width * 4 >= reference * 3 && width * 3 <= reference * 4;
}

/*
 * 從 runs[at]（黑段）開始試著解出一整個條碼。
 * 回傳字元數；RESULT_PARTIAL 表示起始碼和幾個符號都對，但條碼延伸到影像邊緣外。
 */
static int decode_at(const uint16_t *runs, int count, int at, char *out, size_t out_size)
{
    if (at + SYMBOL_RUNS > count) {
        return RESULT_NONE;
    }
    const int start = best_symbol(runs + at, START_A, START_C);
    if (start < 0) {
        return RESULT_NONE;
    }
    /* 起始碼前面必須有空白（至少半個符號寬） */
    const int start_width = group_width(runs + at);
    if (at == 0 || runs[at - 1] < start_width / 2) {
        return RESULT_NONE;
    }

    int values[MAX_SYMBOLS];
    int n = 0;
    int pos = at + SYMBOL_RUNS;
    for (;;) {
        const int partial = n >= PARTIAL_MIN_SYMBOLS ? RESULT_PARTIAL : RESULT_NONE;
        if (pos + SYMBOL_RUNS > count) {
            return partial;  /* 條碼還沒結束，影像已經到邊緣 */
        }
        const int v = best_symbol(runs + pos, 0, CODE_STOP);
        if (v < 0 || !width_matches(group_width(runs + pos), start_width)) {
            /* 這一組的最後一段碰到影像邊緣時，寬度不可信：也算條碼被切掉 */
            return pos + SYMBOL_RUNS == count ? partial : RESULT_NONE;
        }
        if (v == CODE_STOP) {
            break;
        }
        if (v >= START_A || n >= MAX_SYMBOLS) {
            return RESULT_NONE;  /* 起始碼不會出現在中間 */
        }
        values[n++] = v;
        pos += SYMBOL_RUNS;
    }
    /* 結束碼是 7 段：再確認一次 */
    if (pos + STOP_RUNS > count) {
        return n >= PARTIAL_MIN_SYMBOLS ? RESULT_PARTIAL : RESULT_NONE;
    }
    if (variance(runs + pos, PATTERNS[CODE_STOP], STOP_RUNS, 13) >= MAX_AVG_VARIANCE) {
        return RESULT_NONE;
    }
    if (n < 2) {
        return RESULT_NONE;  /* 至少一個資料符號加檢查碼 */
    }
    long sum = start;
    for (int i = 0; i < n - 1; i++) {
        sum += (long)values[i] * (i + 1);
    }
    if (sum % CHECK_MODULUS != values[n - 1]) {
        return RESULT_NONE;
    }
    const int len = to_text(start, values, n - 1, out, out_size);
    return len > 0 ? len : RESULT_NONE;
}

/*
 * runs[at] 是結束碼的第一段嗎？是的話往前數符號。
 * 前面的符號一路接到影像邊緣（找不到起始碼）：條碼的開頭在畫面外。
 */
static bool is_tail_of_cut_barcode(const uint16_t *runs, int count, int at)
{
    if (at + STOP_RUNS > count || at < SYMBOL_RUNS ||
        variance(runs + at, PATTERNS[CODE_STOP], STOP_RUNS, 13) >= MAX_AVG_VARIANCE) {
        return false;
    }
    int stop_width = 0;
    for (int i = 0; i < STOP_RUNS; i++) {
        stop_width += runs[at + i];
    }
    /* 結束碼後面必須有空白，或已經是最後一段 */
    if (at + STOP_RUNS < count && runs[at + STOP_RUNS] < stop_width / 2) {
        return false;
    }
    const int reference = stop_width * SYMBOL_UNITS / 13;
    int symbols = 0;
    int pos = at - SYMBOL_RUNS;
    while (pos >= 1) {
        const int v = best_symbol(runs + pos, 0, START_C);
        if (v < 0 || !width_matches(group_width(runs + pos), reference)) {
            return false;  /* 中間有不是符號的東西：不是條碼 */
        }
        if (v >= START_A) {
            return false;  /* 起始碼在畫面裡：整條都拍到了，讀不出來是別的原因 */
        }
        symbols++;
        pos -= SYMBOL_RUNS;
    }
    return symbols >= PARTIAL_MIN_SYMBOLS;
}

static int decode_runs(const uint16_t *runs, int count, char *out, size_t out_size)
{
    bool partial = false;
    /* runs[0] 是白段，所以奇數位置是黑段 */
    for (int at = 1; at + SYMBOL_RUNS <= count; at += 2) {
        const int len = decode_at(runs, count, at, out, out_size);
        if (len > 0) {
            return len;
        }
        partial = partial || len == RESULT_PARTIAL || is_tail_of_cut_barcode(runs, count, at);
    }
    return partial ? RESULT_PARTIAL : RESULT_NONE;
}

code128_result_t code128_scan_row(const uint8_t *gray, int n, char *out, size_t out_size)
{
    if (gray == NULL || out == NULL || out_size == 0 || n <= 0 || n > 4096) {
        return CODE128_NONE;
    }
    out[0] = '\0';
    uint16_t runs[MAX_RUNS];
    const int count = to_runs(gray, n, runs, MAX_RUNS);
    if (count < SYMBOL_RUNS * 3) {
        return CODE128_NONE;
    }
    const int forward = decode_runs(runs, count, out, out_size);
    if (forward > 0) {
        return CODE128_OK;
    }
    /* 反方向（條碼上下顛倒時）。段數是偶數時最後一段是黑的，補一個 0 寬的白段在前面 */
    uint16_t rev[MAX_RUNS + 1];
    int rc = 0;
    if (count % 2 == 0) {
        rev[rc++] = 0;
    }
    for (int i = count - 1; i >= 0; i--) {
        rev[rc++] = runs[i];
    }
    const int backward = decode_runs(rev, rc, out, out_size);
    if (backward > 0) {
        return CODE128_OK;
    }
    out[0] = '\0';
    return forward == RESULT_PARTIAL || backward == RESULT_PARTIAL ? CODE128_PARTIAL
                                                                   : CODE128_NONE;
}

int code128_decode_row(const uint8_t *gray, int n, char *out, size_t out_size)
{
    if (code128_scan_row(gray, n, out, out_size) != CODE128_OK) {
        return -1;
    }
    return (int)strlen(out);
}
