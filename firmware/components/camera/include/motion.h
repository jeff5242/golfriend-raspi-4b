#pragma once
/*
 * 鏡頭畫面的移動偵測，以及「有動靜才顯示預覽」的計時。
 * 純邏輯，不依賴 ESP-IDF，可以在主機端測試。
 *
 * 判斷方式：灰階影像切成 MOTION_GRID_X x MOTION_GRID_Y 格，每格取平均亮度，和上一張比。
 * 比之前先扣掉整張的平均亮度變化：自動曝光調整或燈光整體變亮變暗時，每一格會一起變，
 * 不算移動。扣掉之後還有 MOTION_MIN_BLOCKS 格以上差超過 MOTION_BLOCK_DIFF，才算畫面有變動。
 */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOTION_GRID_X     16
#define MOTION_GRID_Y     9
#define MOTION_BLOCK_DIFF 12  /* 一格的平均亮度差超過這個值（0–255）才算這格有變 */
#define MOTION_MIN_BLOCKS 3   /* 至少幾格有變才算畫面有變動（一格約佔畫面 0.7%） */

typedef struct {
    uint8_t prev[MOTION_GRID_X * MOTION_GRID_Y];
    bool has_prev;
    bool active;            /* 目前要不要顯示預覽 */
    uint32_t last_activity_ms;
} motion_t;

/*
 * 和上一張比，畫面有沒有變動。gray 是 w x h 的 8 位元灰階。
 * 第一張（沒有上一張可比）回傳 false。w、h 小於格數時回傳 false。
 */
bool motion_detect(motion_t *m, const uint8_t *gray, int w, int h);

/*
 * 更新「要不要顯示預覽」。activity 為 true（畫面有變動、或讀到碼）時立刻顯示並重新計時；
 * 最後一次 activity 之後超過 hold_ms 就隱藏。now_ms 可以溢位繞回。回傳目前要不要顯示。
 */
bool motion_activity(motion_t *m, bool activity, uint32_t now_ms, uint32_t hold_ms);

#ifdef __cplusplus
}
#endif
