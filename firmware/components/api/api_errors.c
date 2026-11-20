#include "api_errors.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    const char *code;
    const char *text;
} error_entry_t;

static const error_entry_t TAKE_BALL_ERRORS[] = {
    {"1", "輸入錯誤"},
    {"2", "timeout"},
    {"3", "加密字串錯誤"},
    {"4", "裝置不存在"},
    {"5", "取球碼不存在"},
    {"6", "領球碼過期，請重新產生"},
    {"9", "伺服器錯誤"},
};

static const error_entry_t CONFIRM_ERRORS[] = {
    {"1", "輸入錯誤"},
    {"2", "timeout"},
    {"3", "加密字串錯誤"},
    {"4", "交易序號不存在"},
    {"5", "已設為成功 不得重複呼叫"},
    {"6", "已設為失敗 不得重複呼叫"},
    {"9", "伺服器錯誤"},
};

#define UNKNOWN_ERROR "Unknow Error"
#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

static const char *lookup(const error_entry_t *table, size_t n, const char *code)
{
    for (size_t i = 0; i < n; i++) {
        if (strcmp(table[i].code, code) == 0) {
            return table[i].text;
        }
    }
    return UNKNOWN_ERROR;
}

const char *api_error_text(api_cmd_t cmd, const char *code)
{
    if (code == NULL) {
        return UNKNOWN_ERROR;
    }
    if (cmd == API_CMD_TAKE_BALL) {
        return lookup(TAKE_BALL_ERRORS, COUNT_OF(TAKE_BALL_ERRORS), code);
    }
    if (cmd == API_CMD_TAKE_BALL_CONFIRM) {
        return lookup(CONFIRM_ERRORS, COUNT_OF(CONFIRM_ERRORS), code);
    }
    return UNKNOWN_ERROR;
}
