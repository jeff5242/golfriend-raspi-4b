#pragma once
/*
 * QR Code 內容的處理。純邏輯，可以在主機端測試。
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QR_CODE_MAX 32  /* 領球碼最長 31 字；和掃碼器一行的上限一致 */

/*
 * 把 QR Code 的內容轉成領球碼：只留數字，其他字元全部拿掉（例如前綴 '%'、空白、換行）。
 * 沒有任何數字、或數字超過 QR_CODE_MAX - 1 位時回傳 false。
 */
bool qr_code_from_payload(const uint8_t *payload, size_t len, char *out, size_t out_size);

/*
 * 防止同一張 QR Code 被重複送出。
 * 客人把手機停在鏡頭前時，每一張影像都會讀到同一組碼；只有第一次要送出。
 * 同一組碼離開鏡頭超過 hold_ms 之後，才可以再送一次。
 */
typedef struct {
    char last[QR_CODE_MAX];
    uint32_t last_seen_ms;
    bool has_last;
} qr_gate_t;

/* 回傳 true 表示這一次要送出。now_ms 可以溢位繞回。 */
bool qr_gate_accept(qr_gate_t *gate, const char *code, uint32_t now_ms, uint32_t hold_ms);

/*
 * RGB565（小端，每像素 2 bytes）縮成 8 位元灰階：寬高各取一半，用綠色通道當亮度。
 * dst 必須有 (w/2)*(h/2) bytes。mirror 為 true 時每一列左右對調。
 */
void qr_gray_half_from_rgb565(const uint8_t *src, int w, int h, uint8_t *dst, bool mirror);

/*
 * 給畫面看的掃描狀態。每張影像的結果會跳動，所以狀態至少維持 hold_ms 才往回退。
 */
typedef enum {
    QR_SCAN_IDLE = 0,  /* 影像裡沒有 QR Code */
    QR_SCAN_FOUND,     /* 看到 QR Code 的定位點，但還讀不出內容 */
    QR_SCAN_READ,      /* 讀出內容了 */
} qr_scan_state_t;

typedef struct {
    qr_scan_state_t shown;
    uint32_t since_ms;  /* 最後一次觀察到 shown 這個狀態的時間 */
} qr_status_t;

/* found：這張影像有 QR Code；read：這張影像讀出了內容。回傳畫面要顯示的狀態。 */
qr_scan_state_t qr_status_update(qr_status_t *st, bool found, bool read, uint32_t now_ms,
                                 uint32_t hold_ms);

#ifdef __cplusplus
}
#endif
