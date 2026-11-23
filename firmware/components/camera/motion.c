#include "motion.h"

#include <stdlib.h>
#include <string.h>

#define MOTION_BLOCKS (MOTION_GRID_X * MOTION_GRID_Y)

/* 每格的平均亮度。邊緣除不盡的像素不算。 */
static void block_means(const uint8_t *gray, int w, int h, uint8_t *out)
{
    const int bw = w / MOTION_GRID_X;
    const int bh = h / MOTION_GRID_Y;
    for (int by = 0; by < MOTION_GRID_Y; by++) {
        for (int bx = 0; bx < MOTION_GRID_X; bx++) {
            uint32_t sum = 0;
            for (int y = by * bh; y < (by + 1) * bh; y++) {
                const uint8_t *row = gray + (size_t)y * (size_t)w + (size_t)(bx * bw);
                for (int x = 0; x < bw; x++) {
                    sum += row[x];
                }
            }
            out[by * MOTION_GRID_X + bx] = (uint8_t)(sum / (uint32_t)(bw * bh));
        }
    }
}

bool motion_detect(motion_t *m, const uint8_t *gray, int w, int h)
{
    if (m == NULL || gray == NULL || w < MOTION_GRID_X || h < MOTION_GRID_Y) {
        return false;
    }
    uint8_t cur[MOTION_BLOCKS];
    block_means(gray, w, h, cur);
    if (!m->has_prev) {
        memcpy(m->prev, cur, sizeof cur);
        m->has_prev = true;
        return false;
    }
    /* 整張的平均變化：自動曝光或燈光造成的整體變亮變暗 */
    int32_t shift = 0;
    for (int i = 0; i < MOTION_BLOCKS; i++) {
        shift += (int32_t)cur[i] - (int32_t)m->prev[i];
    }
    shift /= MOTION_BLOCKS;
    int changed = 0;
    for (int i = 0; i < MOTION_BLOCKS; i++) {
        const int32_t d = (int32_t)cur[i] - (int32_t)m->prev[i] - shift;
        if (abs(d) > MOTION_BLOCK_DIFF) {
            changed++;
        }
    }
    memcpy(m->prev, cur, sizeof cur);
    return changed >= MOTION_MIN_BLOCKS;
}

bool motion_activity(motion_t *m, bool activity, uint32_t now_ms, uint32_t hold_ms)
{
    if (m == NULL) {
        return false;
    }
    if (activity) {
        m->active = true;
        m->last_activity_ms = now_ms;
    } else if (m->active && (uint32_t)(now_ms - m->last_activity_ms) > hold_ms) {
        m->active = false;  /* 無號數相減：now_ms 溢位繞回時結果仍然正確 */
    }
    return m->active;
}
