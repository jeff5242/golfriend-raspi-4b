#include "scanner_devs.h"

#include <string.h>

#define HID_BOOT_KBD_REPORT_LEN 8

void scanner_devs_init(scanner_devs_t *devs)
{
    if (devs != NULL) {
        memset(devs, 0, sizeof *devs);
    }
}

scanner_dev_t *scanner_devs_get(scanner_devs_t *devs, const void *key, uint32_t reset_gen)
{
    if (devs == NULL || key == NULL) {
        return NULL;
    }
    scanner_dev_t *free_slot = NULL;
    for (size_t i = 0; i < SCANNER_MAX_DEVICES; i++) {
        scanner_dev_t *dev = &devs->slot[i];
        if (dev->key == key) {
            return dev;
        }
        if (dev->key == NULL && free_slot == NULL) {
            free_slot = dev;
        }
    }
    if (free_slot != NULL) {
        memset(free_slot, 0, sizeof *free_slot);
        free_slot->key = key;
        free_slot->reset_gen = reset_gen;
        scanner_line_init(&free_slot->line);
    }
    return free_slot;
}

void scanner_devs_remove(scanner_devs_t *devs, const void *key)
{
    if (devs == NULL || key == NULL) {
        return;
    }
    for (size_t i = 0; i < SCANNER_MAX_DEVICES; i++) {
        if (devs->slot[i].key == key) {
            memset(&devs->slot[i], 0, sizeof devs->slot[i]);
        }
    }
}

size_t scanner_dev_report_chars(scanner_dev_t *dev, const uint8_t *report, size_t len,
                                char out[HID_KBD_KEY_SLOTS])
{
    if (dev == NULL || report == NULL || out == NULL || len < HID_BOOT_KBD_REPORT_LEN) {
        return 0;
    }
    const bool shift = hid_keymap_has_shift(report[0]);
    const uint8_t *cur = &report[2];
    uint8_t fresh[HID_KBD_KEY_SLOTS];
    const size_t n = hid_keymap_new_keys(dev->prev_keys, cur, fresh);
    memcpy(dev->prev_keys, cur, HID_KBD_KEY_SLOTS);
    size_t count = 0;
    for (size_t i = 0; i < n; i++) {
        const char c = hid_keymap_to_ascii(fresh[i], shift);
        if (c != 0) {
            out[count++] = c;
        }
    }
    return count;
}

bool scanner_dev_feed(scanner_dev_t *dev, char c, int64_t now_us, uint32_t reset_gen,
                      char *out, size_t out_size)
{
    if (dev == NULL) {
        return false;
    }
    if (dev->reset_gen != reset_gen) {
        dev->reset_gen = reset_gen;
        (void)scanner_line_clear_stale(&dev->line, now_us);
    }
    return scanner_line_feed(&dev->line, c, now_us, out, out_size);
}
