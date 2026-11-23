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
 * RGB565（小端，每像素 2 bytes）縮成 8 位元灰階：寬高各取一半，每 2x2 個像素的綠色通道取平均。
 * dst 必須有 (w/2)*(h/2) bytes。mirror 為 true 時每一列左右對調。
 */
void qr_gray_half_from_rgb565(const uint8_t *src, int w, int h, uint8_t *dst, bool mirror);

/*
 * 取影像正中間 dw x dh 的區塊，不縮小，轉成 8 位元灰階。dst 必須有 dw*dh bytes。
 * dw、dh 不得超過 w、h。
 */
void qr_gray_crop_from_rgb565(const uint8_t *src, int w, int h, int dw, int dh, uint8_t *dst,
                              bool mirror);

/*
 * 給畫面看的掃描狀態。每張影像的結果會跳動，所以狀態至少維持 hold_ms 才往回退。
 */
typedef enum {
    QR_SCAN_IDLE = 0,  /* 影像裡沒有 QR Code */
    QR_SCAN_FOUND,     /* 看到 QR Code 的定位點，但還讀不出內容 */
    QR_SCAN_TOO_CLOSE, /* 一維條碼只拍到半截：手機太近 */
    QR_SCAN_READ,      /* 讀出內容並送出 */
} qr_scan_state_t;

typedef struct {
    qr_scan_state_t shown;
    uint32_t since_ms;  /* 最後一次觀察到 shown 這個狀態的時間 */
} qr_status_t;

/* seen：這張影像的結果。回傳畫面要顯示的狀態。 */
qr_scan_state_t qr_status_update(qr_status_t *st, qr_scan_state_t seen, uint32_t now_ms,
                                 uint32_t hold_ms);

#ifdef __cplusplus
}
#endif
