#pragma once
/*
 * 現場設定檔（TF 卡上的 config.txt）的解析。純邏輯，可以在主機端測試。
 *
 * 格式：一行一項 key=value。# 開頭是註解。key 與 value 前後的空白會去掉。
 *   device_code=TGSGF_1F_M1
 *   wifi1_ssid=...        wifi1_password=...
 *   wifi2_ssid=...        wifi2_password=...     （備援，可以不寫）
 *   log_token=...         （Better Stack 的 token，可以不寫）
 *   ota_url=https://...   （韌體更新的版本說明檔，可以不寫）
 * 沒寫的項目維持原值，所以可以只改其中一項。
 */
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SETTINGS_DEVICE_CODE_MAX 48
#define SETTINGS_SSID_MAX        33  /* SSID 最長 32 bytes */
#define SETTINGS_PASSWORD_MAX    65  /* WPA2 密碼最長 64 bytes */
#define SETTINGS_LOG_TOKEN_MAX   64
#define SETTINGS_OTA_URL_MAX     160

typedef struct {
    char device_code[SETTINGS_DEVICE_CODE_MAX];
    char wifi1_ssid[SETTINGS_SSID_MAX];
    char wifi1_password[SETTINGS_PASSWORD_MAX];
    char wifi2_ssid[SETTINGS_SSID_MAX];
    char wifi2_password[SETTINGS_PASSWORD_MAX];
    char log_token[SETTINGS_LOG_TOKEN_MAX];  /* Better Stack token；空字串＝不送遠端 log */
    char ota_url[SETTINGS_OTA_URL_MAX];  /* 版本說明檔的網址；空字串＝不做遠端更新 */
} settings_t;

typedef struct {
    int applied;   /* 成功套用的項目數 */
    int rejected;  /* 不認識的 key、沒有 '='、或值太長的行數 */
} settings_parse_result_t;

/*
 * 把 text（以 0 結尾）裡的設定套用到 out。out 原有的值在沒被設定檔提到時保留。
 * 值太長的行整行不套用（不截斷：截斷的密碼或裝置代碼比沒有更難查）。
 */
settings_parse_result_t settings_parse(const char *text, settings_t *out);

#ifdef __cplusplus
}
#endif
