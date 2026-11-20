#include "hid_keymap.h"

#include <stdbool.h>

#define HID_MOD_LSHIFT 0x02
#define HID_MOD_RSHIFT 0x20

#define HID_USAGE_FIRST_KEY 0x04  /* 小於這個值是「無按鍵／ErrorRollOver」 */
#define HID_USAGE_A         0x04
#define HID_USAGE_Z         0x1D
#define HID_USAGE_1         0x1E
#define HID_USAGE_9         0x26
#define HID_USAGE_0         0x27
#define HID_USAGE_ENTER     0x28
#define HID_USAGE_TAB       0x2B
#define HID_USAGE_SPACE     0x2C
#define HID_USAGE_MINUS     0x2D
#define HID_USAGE_EQUAL     0x2E
#define HID_USAGE_PERIOD    0x37
#define HID_USAGE_SLASH     0x38
#define HID_USAGE_KP_ENTER  0x58
#define HID_USAGE_KP_1      0x59
#define HID_USAGE_KP_9      0x61
#define HID_USAGE_KP_0      0x62

bool hid_keymap_has_shift(uint8_t modifier)
{
    return (modifier & (HID_MOD_LSHIFT | HID_MOD_RSHIFT)) != 0;
}

char hid_keymap_to_ascii(uint8_t usage, bool shift)
{
    static const char shifted_digits[] = "!@#$%^&*(";

    if (usage >= HID_USAGE_A && usage <= HID_USAGE_Z) {
        return (char)((shift ? 'A' : 'a') + (usage - HID_USAGE_A));
    }
    if (usage >= HID_USAGE_1 && usage <= HID_USAGE_9) {
        return shift ? shifted_digits[usage - HID_USAGE_1] : (char)('1' + (usage - HID_USAGE_1));
    }
    if (usage >= HID_USAGE_KP_1 && usage <= HID_USAGE_KP_9) {
        return (char)('1' + (usage - HID_USAGE_KP_1));
    }

    switch (usage) {
    case HID_USAGE_0:        return shift ? ')' : '0';
    case HID_USAGE_KP_0:     return '0';
    case HID_USAGE_ENTER:    return '\n';
    case HID_USAGE_KP_ENTER: return '\n';
    case HID_USAGE_TAB:      return '\t';
    case HID_USAGE_SPACE:    return ' ';
    case HID_USAGE_MINUS:    return shift ? '_' : '-';
    case HID_USAGE_EQUAL:    return shift ? '+' : '=';
    case HID_USAGE_PERIOD:   return shift ? '>' : '.';
    case HID_USAGE_SLASH:    return shift ? '?' : '/';
    default:                 return 0;
    }
}

static bool contains_key(const uint8_t keys[HID_KBD_KEY_SLOTS], uint8_t usage)
{
    for (size_t i = 0; i < HID_KBD_KEY_SLOTS; i++) {
        if (keys[i] == usage) {
            return true;
        }
    }
    return false;
}

size_t hid_keymap_new_keys(const uint8_t prev[HID_KBD_KEY_SLOTS],
                           const uint8_t cur[HID_KBD_KEY_SLOTS],
                           uint8_t out[HID_KBD_KEY_SLOTS])
{
    size_t n = 0;
    for (size_t i = 0; i < HID_KBD_KEY_SLOTS; i++) {
        const uint8_t usage = cur[i];
        if (usage < HID_USAGE_FIRST_KEY || contains_key(prev, usage)) {
            continue;
        }
        out[n++] = usage;
    }
    return n;
}
