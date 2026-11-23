#pragma once
/*
 * 遠端 log：把重要事件批次送到 Better Stack（Logtail），比照 Pi 版 log.py。
 * 背景工作傳送，不阻塞呼叫端；佇列滿或沒有網路時丟掉，不影響領球。
 * token 來自現場設定（TF 卡 config.txt 的 log_token）或 menuconfig，不得寫進程式碼。
 */
#include "esp_err.h"
#include "remote_log_format.h"

#ifdef __cplusplus
extern "C" {
#endif

/* token 是空字串時不啟動，之後的 remote_log_event() 都直接略過。字串內容會被複製。 */
esp_err_t remote_log_start(const char *token, const char *device);

/* 記一筆事件。level 用 "INFO"／"WARNING"／"ERROR"。可以從任何工作呼叫。不得放密碼。 */
void remote_log_event(remote_log_tag_t tag, const char *level, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

#ifdef __cplusplus
}
#endif
