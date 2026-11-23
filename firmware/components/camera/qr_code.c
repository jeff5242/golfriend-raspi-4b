#include "qr_code.h"

#include <string.h>

static bool is_affix(uint8_t c)
{
    return c == '%' || c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool qr_code_from_payload(const uint8_t *payload, size_t len, char *out, size_t out_size)
{
    if (payload == NULL || out == NULL || out_size == 0) {
        return false;
    }
    size_t begin = 0;
    size_t end = len;
    while (begin < end && is_affix(payload[begin])) {
        begin++;
    }
    while (end > begin && is_affix(payload[end - 1])) {
        end--;
    }
    const size_t n = end - begin;
    if (n == 0 || n >= out_size || n >= QR_CODE_MAX) {
        return false;
    }
    for (size_t i = begin; i < end; i++) {
        if (payload[i] <= ' ' || payload[i] > '~') {
            return false;  /* 領球碼只會是可見的 ASCII 字元 */
        }
    }
    memcpy(out, payload + begin, n);
    out[n] = '\0';
    return true;
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

void qr_gray_half_from_rgb565(const uint8_t *src, int w, int h, uint8_t *dst)
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
            *dst++ = (uint8_t)(((px >> 5) & 0x3F) << 2);
        }
    }
}
