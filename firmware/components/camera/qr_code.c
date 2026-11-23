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

bool qr_gate_accept(qr_gate_t *gate, const char *code, uint32_t now_ms, uint32_t hold_ms)
{
    if (gate == NULL || code == NULL || code[0] == '\0' || strlen(code) >= QR_CODE_MAX) {
        return false;
    }
    const bool same = gate->has_last && strcmp(gate->last, code) == 0;
    /* 無號數相減：now_ms 溢位繞回時結果仍然正確 */
    const bool still_held = same && (uint32_t)(now_ms - gate->last_seen_ms) <= hold_ms;
    strcpy(gate->last, code);
    gate->last_seen_ms = now_ms;
    gate->has_last = true;
    return !still_held;
}

void qr_gray_half_from_rgb565(const uint8_t *src, int w, int h, uint8_t *dst, bool mirror)
{
    if (src == NULL || dst == NULL) {
        return;
    }
    const int dw = w / 2;
    const int dh = h / 2;
    for (int y = 0; y < dh; y++) {
        const uint8_t *row = src + (size_t)(y * 2) * (size_t)w * 2;
        for (int x = 0; x < dw; x++) {
            const uint8_t *p = row + (size_t)(x * 2) * 2;
            const uint16_t px = (uint16_t)(p[0] | (p[1] << 8));
            dst[mirror ? dw - 1 - x : x] = (uint8_t)(((px >> 5) & 0x3F) << 2);
        }
        dst += dw;
    }
}

qr_scan_state_t qr_status_update(qr_status_t *st, bool found, bool read, uint32_t now_ms,
                                 uint32_t hold_ms)
{
    if (st == NULL) {
        return QR_SCAN_IDLE;
    }
    const qr_scan_state_t seen = read ? QR_SCAN_READ : found ? QR_SCAN_FOUND : QR_SCAN_IDLE;
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
