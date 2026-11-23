#include "settings_parse.h"

#include <string.h>

typedef struct {
    const char *key;
    size_t offset;
    size_t size;
} field_t;

#define FIELD(name) {#name, offsetof(settings_t, name), sizeof(((settings_t *)0)->name)}

static const field_t s_fields[] = {
    FIELD(device_code), FIELD(wifi1_ssid), FIELD(wifi1_password),
    FIELD(wifi2_ssid),  FIELD(wifi2_password), FIELD(log_token),
    FIELD(ota_url),
};

static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

/* 去掉 [*start, *end) 前後的空白 */
static void trim(const char **start, const char **end)
{
    while (*start < *end && is_space(**start)) {
        (*start)++;
    }
    while (*end > *start && is_space(*(*end - 1))) {
        (*end)--;
    }
}

static const field_t *find_field(const char *key, size_t key_len)
{
    for (size_t i = 0; i < sizeof s_fields / sizeof s_fields[0]; i++) {
        if (strlen(s_fields[i].key) == key_len && memcmp(s_fields[i].key, key, key_len) == 0) {
            return &s_fields[i];
        }
    }
    return NULL;
}

/* 套用一行。回傳 1：套用；-1：拒絕；0：空行或註解。 */
static int apply_line(const char *line, const char *line_end, settings_t *out)
{
    trim(&line, &line_end);
    if (line == line_end || *line == '#') {
        return 0;
    }
    const char *eq = memchr(line, '=', (size_t)(line_end - line));
    if (eq == NULL) {
        return -1;
    }
    const char *key = line;
    const char *key_end = eq;
    const char *value = eq + 1;
    const char *value_end = line_end;
    trim(&key, &key_end);
    trim(&value, &value_end);

    const field_t *field = find_field(key, (size_t)(key_end - key));
    const size_t value_len = (size_t)(value_end - value);
    if (field == NULL || value_len >= field->size) {
        return -1;
    }
    char *dst = (char *)out + field->offset;
    memcpy(dst, value, value_len);
    dst[value_len] = '\0';
    return 1;
}

settings_parse_result_t settings_parse(const char *text, settings_t *out)
{
    settings_parse_result_t result = {0, 0};
    if (text == NULL || out == NULL) {
        return result;
    }
    if (strncmp(text, "\xEF\xBB\xBF", 3) == 0) {
        text += 3;  /* UTF-8 BOM（Windows 記事本） */
    }
    while (*text != '\0') {
        const char *line_end = strchr(text, '\n');
        if (line_end == NULL) {
            line_end = text + strlen(text);
        }
        const int r = apply_line(text, line_end, out);
        if (r > 0) {
            result.applied++;
        } else if (r < 0) {
            result.rejected++;
        }
        text = *line_end == '\0' ? line_end : line_end + 1;
    }
    return result;
}
