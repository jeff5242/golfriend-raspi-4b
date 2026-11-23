#include "qr_code.h"

#include <string.h>

bool qr_code_from_payload(const uint8_t *payload, size_t len, char *out, size_t out_size)
{
    if (payload == NULL || out == NULL || out_size == 0) {
        return false;
    }
    size_t n = 0;
    for (size_t i = 0; i < len; i++) {
        if (payload[i] < '0' || payload[i] > '9') {
            continue;  /* 非數字的字元全部拿掉（前綴 '%'、空白、換行、其他符號） */
        }
        if (n + 1 >= out_size || n + 1 >= QR_CODE_MAX) {
            out[0] = '\0';
            return false;  /* 太長：不是領球碼 */
        }
        out[n++] = (char)payload[i];
    }
    out[n] = '\0';
    return n > 0;
}

/* RGB565 的綠色通道（6 位元） */
static inline unsigned green6(const uint8_t *p)
{
    return ((unsigned)(p[0] | (p[1] << 8)) >> 5) & 0x3F;
}

void qr_gray_half_from_rgb565(const uint8_t *src, int w, int h, uint8_t *dst, bool mirror)
{
    if (src == NULL || dst == NULL) {
        return;
    }
    const int dw = w / 2;
    const int dh = h / 2;
    const size_t stride = (size_t)w * 2;
    for (int y = 0; y < dh; y++) {
        const uint8_t *row0 = src + (size_t)(y * 2) * stride;
        const uint8_t *row1 = row0 + stride;
        for (int x = 0; x < dw; x++) {
            const size_t off = (size_t)(x * 2) * 2;
            /* 四個 6 位元的值相加是 8 位元：等於平均後再放大成 0–252 */
            const unsigned sum = green6(row0 + off) + green6(row0 + off + 2) +
                                 green6(row1 + off) + green6(row1 + off + 2);
            dst[mirror ? dw - 1 - x : x] = (uint8_t)sum;
        }
        dst += dw;
    }
}

void qr_gray_crop_from_rgb565(const uint8_t *src, int w, int h, int dw, int dh, uint8_t *dst,
                              bool mirror)
{
    if (src == NULL || dst == NULL || dw <= 0 || dh <= 0 || dw > w || dh > h) {
        return;
    }
    const int x0 = (w - dw) / 2;
    const int y0 = (h - dh) / 2;
    for (int y = 0; y < dh; y++) {
        const uint8_t *row = src + ((size_t)(y0 + y) * (size_t)w + (size_t)x0) * 2;
        for (int x = 0; x < dw; x++) {
            dst[mirror ? dw - 1 - x : x] = (uint8_t)(green6(row + (size_t)x * 2) << 2);
        }
        dst += dw;
    }
}

qr_scan_state_t qr_status_update(qr_status_t *st, qr_scan_state_t seen, uint32_t now_ms,
                                 uint32_t hold_ms)
{
    if (st == NULL) {
        return QR_SCAN_IDLE;
    }
    const bool holding = (uint32_t)(now_ms - st->since_ms) <= hold_ms;
    if (seen >= st->shown) {
        if (seen != QR_SCAN_IDLE || st->shown != QR_SCAN_IDLE) {
            st->since_ms = now_ms;  /* 同一個或更高的狀態：重新計時 */
        }
        st->shown = seen;
    } else if (!holding) {
        st->shown = seen;
        st->since_ms = now_ms;
    }
    return st->shown;
}
