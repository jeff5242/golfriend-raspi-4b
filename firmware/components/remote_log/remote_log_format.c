#include "remote_log_format.h"

#include <stdbool.h>
#include <stdio.h>

typedef struct {
    char *out;
    size_t size;
    size_t len;
    bool overflow;
} writer_t;

static void put(writer_t *w, char c)
{
    if (w->len + 1 >= w->size) {
        w->overflow = true;
        return;
    }
    w->out[w->len++] = c;
}

static void put_raw(writer_t *w, const char *s)
{
    for (; *s != '\0'; s++) {
        put(w, *s);
    }
}

/* JSON 字串內容：跳脫 " \ 與控制字元；0x80 以上（UTF-8）原樣輸出 */
static void put_escaped(writer_t *w, const char *s)
{
    for (; *s != '\0'; s++) {
        const unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') {
            put(w, '\\');
            put(w, (char)c);
        } else if (c == '\n') {
            put_raw(w, "\\n");
        } else if (c == '\r') {
            put_raw(w, "\\r");
        } else if (c == '\t') {
            put_raw(w, "\\t");
        } else if (c < 0x20) {
            char buf[8];
            snprintf(buf, sizeof buf, "\\u%04x", c);
            put_raw(w, buf);
        } else {
            put(w, (char)c);
        }
    }
}

static const char *tag_name(remote_log_tag_t tag)
{
    switch (tag) {
    case REMOTE_LOG_SYSTEM:    return "system";
    case REMOTE_LOG_OPERATION: return "operation";
    case REMOTE_LOG_EXCEPTION: return "exception";
    case REMOTE_LOG_INFO:      return "info";
    }
    return "info";
}

/* 同 Pi 版 TAG_LABELS */
static const char *tag_label(remote_log_tag_t tag)
{
    switch (tag) {
    case REMOTE_LOG_SYSTEM:    return "系統";
    case REMOTE_LOG_OPERATION: return "操作";
    case REMOTE_LOG_EXCEPTION: return "例外";
    case REMOTE_LOG_INFO:      return "資訊";
    }
    return "資訊";
}

int remote_log_format_entry(char *out, size_t out_size, const remote_log_entry_t *entry)
{
    if (out == NULL || out_size == 0 || entry == NULL || entry->dt == NULL ||
        entry->level == NULL || entry->device == NULL || entry->message == NULL) {
        return -1;
    }
    writer_t w = {out, out_size, 0, false};
    put_raw(&w, "{\"dt\":\"");
    put_escaped(&w, entry->dt);
    put_raw(&w, "\",\"level\":\"");
    put_escaped(&w, entry->level);
    put_raw(&w, "\",\"message\":\"[");
    put_escaped(&w, entry->device);
    put_raw(&w, "] [");
    put_raw(&w, tag_label(entry->tag));
    put_raw(&w, "] ");
    put_escaped(&w, entry->message);
    put_raw(&w, "\",\"device\":\"");
    put_escaped(&w, entry->device);
    put_raw(&w, "\",\"tag\":\"");
    put_raw(&w, tag_name(entry->tag));
    put_raw(&w, "\"}");
    if (w.overflow) {
        return -1;
    }
    out[w.len] = '\0';
    return (int)w.len;
}
