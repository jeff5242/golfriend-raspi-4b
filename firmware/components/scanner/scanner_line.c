#include "scanner_line.h"

#include <string.h>

static bool is_terminator(char c)
{
    return c == '\n' || c == '\r' || c == '\t' || c == '%';
}

static void reset_buffer(scanner_line_t *line)
{
    line->len = 0;
    line->overflow = false;
    line->buf[0] = '\0';
}

void scanner_line_init(scanner_line_t *line)
{
    reset_buffer(line);
    line->guard_until_us = 0;
}

bool scanner_line_feed(scanner_line_t *line, char c, int64_t now_us,
                       char *out, size_t out_size)
{
    if (line == NULL || out == NULL || out_size < SCANNER_LINE_MAX) {
        return false;
    }
    if (now_us < line->guard_until_us) {
        return false;
    }

    if (!is_terminator(c)) {
        if (line->len >= SCANNER_LINE_MAX - 1) {
            line->overflow = true;  /* 整行作廢，等結束字元再重來 */
        } else {
            line->buf[line->len++] = c;
        }
        return false;
    }

    const bool usable = !line->overflow && line->len > 0;
    if (usable) {
        memcpy(out, line->buf, line->len);
        out[line->len] = '\0';
        line->guard_until_us = now_us + SCANNER_GUARD_US;
    }
    reset_buffer(line);
    return usable;
}
