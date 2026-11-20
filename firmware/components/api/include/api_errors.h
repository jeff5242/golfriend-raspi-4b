#pragma once
/*
 * 後台 error_msg 代碼 → 畫面顯示的中文。
 * 純邏輯，可以在主機端測試。內容照抄 HttpCmdLibrary.py 的
 * SendTakeBallErrorSet() 與 SendTakeBallConfirmErrorSet()。
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    API_CMD_TAKE_BALL = 0,
    API_CMD_TAKE_BALL_CONFIRM,
} api_cmd_t;

/* code 為 NULL 或不認得時回傳 "Unknow Error"（與 Pi 版相同字樣）。 */
const char *api_error_text(api_cmd_t cmd, const char *code);

#ifdef __cplusplus
}
#endif
