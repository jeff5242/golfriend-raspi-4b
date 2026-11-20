#include "api_sign.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* 與 HttpCmdLibrary.py 相同的固定前綴 */
#define API_SIGN_PREFIX "salt"

int api_sign_input(char *out, size_t out_size,
                   const char *time_str, const char *part1, const char *part2)
{
    if (out == NULL || out_size == 0 || time_str == NULL || part1 == NULL) {
        return -1;
    }
    const int n = snprintf(out, out_size, "%s%s%s%s", API_SIGN_PREFIX, time_str, part1,
                           part2 != NULL ? part2 : "");
    if (n < 0 || (size_t)n >= out_size) {
        out[0] = '\0';
        return -1;
    }
    return n;
}

static bool is_unreserved(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~';
}

/* 把 s 編碼後接在 out[*pos] 之後。空間不足回傳 false。 */
static bool append_encoded(char *out, size_t out_size, size_t *pos, const char *s)
{
    static const char hex[] = "0123456789ABCDEF";
    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; p++) {
        const size_t need = is_unreserved(*p) || *p == ' ' ? 1 : 3;
        if (*pos + need >= out_size) {
            return false;
        }
        if (is_unreserved(*p)) {
            out[(*pos)++] = (char)*p;
        } else if (*p == ' ') {
            out[(*pos)++] = '+';
        } else {
            out[(*pos)++] = '%';
            out[(*pos)++] = hex[*p >> 4];
            out[(*pos)++] = hex[*p & 0x0F];
        }
    }
    return true;
}

static bool append_char(char *out, size_t out_size, size_t *pos, char c)
{
    if (*pos + 1 >= out_size) {
        return false;
    }
    out[(*pos)++] = c;
    return true;
}

int api_form_encode(char *out, size_t out_size,
                    const char *const keys[], const char *const values[], size_t n)
{
    if (out == NULL || out_size == 0 || keys == NULL || values == NULL) {
        return -1;
    }
    size_t pos = 0;
    for (size_t i = 0; i < n; i++) {
        if (keys[i] == NULL || values[i] == NULL) {
            out[0] = '\0';
            return -1;
        }
        const bool ok = (i == 0 || append_char(out, out_size, &pos, '&')) &&
                        append_encoded(out, out_size, &pos, keys[i]) &&
                        append_char(out, out_size, &pos, '=') &&
                        append_encoded(out, out_size, &pos, values[i]);
        if (!ok) {
            out[0] = '\0';
            return -1;
        }
    }
    out[pos] = '\0';
    return (int)pos;
}
