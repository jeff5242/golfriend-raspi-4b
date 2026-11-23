#pragma once
/*
 * 板上鏡頭（OV02C10，MIPI-CSI）掃 QR Code。
 * 背景工作持續取影像、辨識；讀到新的領球碼時呼叫 callback（同一張碼只呼叫一次）。
 */
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* code 是去掉前後綴的領球碼，只在 callback 期間有效。callback 在鏡頭工作裡執行，不得久留。 */
typedef void (*camera_code_cb_t)(const char *code, void *ctx);

/*
 * 啟動鏡頭與辨識。必須在觸控的 I2C 匯流排建立之後呼叫（鏡頭的 SCCB 共用那一條）。
 * 找不到鏡頭時回傳錯誤，其他功能照常運作。
 */
esp_err_t camera_start(camera_code_cb_t cb, void *ctx);

/* 給畫面用的回報。callback 都在鏡頭工作裡執行，不得久留；可以是 NULL。 */
typedef struct {
    /* status 是 qr_scan_state_t（見 qr_code.h），只在改變時呼叫 */
    void (*on_status)(int status, void *ctx);
    /* 每隔幾張影像呼叫一次。gray 是 8 位元灰階，只在 callback 期間有效 */
    void (*on_preview)(const uint8_t *gray, int w, int h, void *ctx);
    /*
     * 預覽要不要顯示：畫面有變動或看到碼時 active=true，之後 20 秒沒有變動就 false。
     * 只在改變時呼叫；開機時是 false（還沒呼叫前視為不顯示）。
     */
    void (*on_activity)(bool active, void *ctx);
    void *ctx;
} camera_observer_t;

/* 必須在 camera_start 之前呼叫 */
void camera_set_observer(const camera_observer_t *observer);

/* 除錯用：把下一張灰階影像用十六進位印到主控台（tools/cam_capture.py 接收）。 */
void camera_debug_dump_next(void);

#ifdef __cplusplus
}
#endif
