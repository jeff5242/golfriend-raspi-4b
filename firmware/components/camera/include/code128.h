#pragma once
/*
 * Code 128 一維條碼辨識（App 的領球條碼：Code 128 B 組，內容是 '%' 加領球碼）。
 * 純邏輯，可以在主機端測試。
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CODE128_TEXT_MAX 32

/*
 * 辨識一列灰階像素（橫切過條碼）。正反兩個方向都會試。
 * 起始碼、每個符號的寬度比例、檢查碼（mod 103）、結束碼全部正確才算成功。
 * 成功回傳字元數並把內容寫到 out（以 \0 結尾）；失敗回傳 -1。
 */
int code128_decode_row(const uint8_t *gray, int n, char *out, size_t out_size);

typedef enum {
    CODE128_NONE = 0,  /* 這一列沒有條碼 */
    CODE128_PARTIAL,   /* 有起始碼和至少 3 個符號，但條碼延伸到影像邊緣外：太近了 */
    CODE128_OK,        /* 完整讀出，內容在 out */
} code128_result_t;

/* 同 code128_decode_row，另外回報「只拍到半截」。不是 CODE128_OK 時 out 是空字串。 */
code128_result_t code128_scan_row(const uint8_t *gray, int n, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif
