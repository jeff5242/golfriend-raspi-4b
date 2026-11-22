#pragma once
/*
 * 手動輸入中的字串（畫面回顯用）。純邏輯，可以在主機端測試。
 * 來源：USB 鍵盤的可顯示字元、觸控鍵盤的數字／刪除（'\b'）／確認（'\n'）。
 */
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 把一個字元套用到 typed（容量 size，含結尾 0）。
 * 回傳 true：收到確認而且 typed 不是空的，呼叫端應該把 typed 當成一行送出。
 * 滿了之後多的字元丟掉；其他控制字元略過。
 */
bool app_typed_apply(char *typed, size_t size, char c);

#ifdef __cplusplus
}
#endif
