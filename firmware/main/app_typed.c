#include "app_typed.h"

#include <string.h>

bool app_typed_apply(char *typed, size_t size, char c)
{
    if (typed == NULL || size == 0) {
        return false;
    }
    const size_t len = strlen(typed);
    if (c == '\n') {
        return len > 0;
    }
    if (c == '\b') {
        if (len > 0) {
            typed[len - 1] = '\0';
        }
        return false;
    }
    const bool printable = c >= 0x20 && c < 0x7F;
    if (printable && len + 1 < size) {
        typed[len] = c;
        typed[len + 1] = '\0';
    }
    return false;
}
