#include "ota_policy.h"

#include <stddef.h>
#include <string.h>

#define VERSION_PARTS 3

typedef struct {
    const char *model;
    size_t model_len;
    unsigned long part[VERSION_PARTS];
} version_t;

/* 讀一段十進位數字；沒有數字或太長時回傳 false */
static bool parse_number(const char **p, unsigned long *out)
{
    const char *s = *p;
    unsigned long value = 0;
    int digits = 0;
    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (unsigned long)(*s - '0');
        s++;
        if (++digits > 6) {
            return false;
        }
    }
    if (digits == 0) {
        return false;
    }
    *p = s;
    *out = value;
    return true;
}

static bool parse_version(const char *text, version_t *out)
{
    if (text == NULL) {
        return false;
    }
    const char *dash = strchr(text, '-');
    if (dash == NULL || dash == text) {
        return false;
    }
    out->model = text;
    out->model_len = (size_t)(dash - text);
    const char *p = dash + 1;
    for (int i = 0; i < VERSION_PARTS; i++) {
        if (!parse_number(&p, &out->part[i])) {
            return false;
        }
        if (i < VERSION_PARTS - 1) {
            if (*p != '.') {
                return false;
            }
            p++;
        }
    }
    return *p == '\0';  /* 後面不得有其他字（例如 -dirty） */
}

bool ota_is_newer(const char *current, const char *candidate)
{
    version_t cur;
    version_t cand;
    if (!parse_version(current, &cur) || !parse_version(candidate, &cand)) {
        return false;
    }
    if (cur.model_len != cand.model_len || memcmp(cur.model, cand.model, cur.model_len) != 0) {
        return false;
    }
    for (int i = 0; i < VERSION_PARTS; i++) {
        if (cand.part[i] != cur.part[i]) {
            return cand.part[i] > cur.part[i];
        }
    }
    return false;
}

static bool has_host_after(const char *url, const char *scheme)
{
    const size_t n = strlen(scheme);
    return strncmp(url, scheme, n) == 0 && url[n] != '\0' && url[n] != '/';
}

bool ota_url_allowed(const char *url, bool allow_http)
{
    if (url == NULL) {
        return false;
    }
    return has_host_after(url, "https://") || (allow_http && has_host_after(url, "http://"));
}
