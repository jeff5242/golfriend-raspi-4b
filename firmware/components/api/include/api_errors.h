#pragma once
/*
 * 後台 error_msg 代碼 → 畫面顯示的中文。
 * 純邏輯，可以在主機端測試。內容照抄 HttpCmdLibrary.py 的
 * SendTakeBallErrorSet() 與 SendTakeBallConfirmErrorSet()。
 */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    API_CMD_TAKE_BALL = 0,
    API_CMD_TAKE_BALL_CONFIRM,
} api_cmd_t;

/* code 為 NULL 或不認得時回傳 "Unknow Error"（與 Pi 版相同字樣）。 */
const char *api_error_text(api_cmd_t cmd, const char *code);

/*
 * 呼叫失敗時畫面顯示的文字。
 * server_rejected 為 true：後台回了 error=true，依 code 查表。
 * server_rejected 為 false：連線、逾時、HTTP 狀態碼異常、還沒對時，一律顯示網路異常。
 */
const char *api_failure_text(api_cmd_t cmd, bool server_rejected, const char *code);

#ifdef __cplusplus
}
#endif
