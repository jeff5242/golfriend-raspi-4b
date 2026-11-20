#pragma once
/*
 * API 簽章字串與表單編碼。
 * 純邏輯，不依賴 ESP-IDF，可以在主機端測試。SHA1 本身在 api.c 用 mbedTLS 算。
 *
 * 對應 HttpCmdLibrary.py：
 *   take_ball         sn = sha1("salt" + time + transaction_code + device_code)
 *   take_ball_confirm sn = sha1("salt" + time + response_id)
 *   payload 用 requests.post(url, dict) 送出，也就是 application/x-www-form-urlencoded。
 */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 組出簽章前的字串："salt" + time_str + part1 + part2。
 * part2 可以是 NULL 或空字串（take_ball_confirm 只有 part1）。
 * 回傳寫入的字元數（不含 '\0'）；任何參數為 NULL（part2 除外）或空間不足時回傳 -1。
 */
int api_sign_input(char *out, size_t out_size,
                   const char *time_str, const char *part1, const char *part2);

/*
 * 把 n 組 key/value 編成 "k1=v1&k2=v2"。
 * 規則同 Python requests：英數與 - _ . ~ 原樣，空白轉 '+'，其他轉 %XX（大寫）。
 * 回傳寫入的字元數（不含 '\0'）；參數為 NULL 或空間不足時回傳 -1。
 */
int api_form_encode(char *out, size_t out_size,
                    const char *const keys[], const char *const values[], size_t n);

#ifdef __cplusplus
}
#endif
