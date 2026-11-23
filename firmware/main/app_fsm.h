#pragma once
/*
 * 出球流程狀態機（只管「下一個狀態是什麼」，不做任何 I/O）。
 * 純邏輯，可以在主機端測試。
 *
 *   待機 --按鍵或點螢幕--> 輸入 --整行--> 查詢 --API 成功--> 確認 --確認成功--> 出球 --馬達停--> 待機
 *   待機 --整行（掃碼器一次送完）--> 查詢
 *   輸入 --10 秒逾時--> 待機
 *   查詢／確認 --失敗或逾時--> 錯誤 --顯示完--> 待機
 *   出球 --馬達無法啟動--> 錯誤
 *   待機／輸入 --鏡頭讀到碼--> 已讀取 --5 秒逾時或按確認--> 查詢
 *   已讀取 --按其他鍵--> 輸入（改成手動，不自動送出）
 *   待機／輸入 --維修碼--> 系統選單 --返回或逾時--> 待機
 *   待機 --找到新版--> 更新中 --失敗--> 待機（成功時重開機）
 *   待機 --按 WiFi 設定鍵（沒有網路時才有）--> WiFi 設定 --返回、連線成功或逾時--> 待機
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_ST_STANDBY = 0,
    APP_ST_INPUT,
    APP_ST_QUERY,
    APP_ST_CONFIRM,
    APP_ST_DISPENSE,
    APP_ST_ERROR,
    APP_ST_MENU,     /* 系統選單（維修碼開啟） */
    APP_ST_UPDATE,   /* 韌體更新中 */
    APP_ST_SCANNED,  /* 鏡頭讀到領球碼：顯示在輸入畫面，倒數後自動送出 */
    APP_ST_WIFI,     /* WiFi 設定：掃描、選網路、輸入密碼、連線 */
} app_state_t;

typedef enum {
    APP_EV_KEY = 0,      /* 收到單一按鍵（手動輸入中） */
    APP_EV_LINE,         /* 收到完整一行（交易碼或卡號） */
    APP_EV_TIMEOUT,      /* 目前狀態的計時到了 */
    APP_EV_API_OK,       /* take_ball 成功 */
    APP_EV_API_FAIL,     /* take_ball 或 take_ball_confirm 失敗、沒網路 */
    APP_EV_CONFIRM_OK,   /* take_ball_confirm 成功 */
    APP_EV_MOTOR_DONE,   /* 馬達時序跑完 */
    APP_EV_ERROR_SHOWN,  /* 錯誤畫面顯示完畢 */
    APP_EV_DISPENSE_FAIL, /* 已確認但馬達無法啟動 */
    APP_EV_TOUCH,        /* 點了待機畫面（開啟觸控鍵盤） */
    APP_EV_MAINT,        /* 輸入了維修碼 */
    APP_EV_MENU_CLOSE,   /* 系統選單按了「返回」 */
    APP_EV_UPDATE_START, /* 找到新版韌體，要開始下載 */
    APP_EV_UPDATE_END,   /* 更新失敗或取消（成功時直接重開機） */
    APP_EV_SCAN,         /* 鏡頭讀到領球碼 */
    APP_EV_WIFI_OPEN,    /* 按了待機畫面右上角的 WiFi 設定鍵 */
    APP_EV_WIFI_CLOSE,   /* WiFi 設定按了「返回」，或連線成功 */
    /* 以下只在 WiFi 設定中處理，不換狀態 */
    APP_EV_WIFI_RESCAN,   /* 按了「重新掃描」 */
    APP_EV_WIFI_SCANNED,  /* 掃描完成（text "1" 成功、"0" 失敗） */
    APP_EV_WIFI_CONNECT,  /* 選好網路、輸入完密碼 */
    APP_EV_WIFI_ACTIVITY, /* 在 WiFi 設定畫面操作（打字、點選）：重新計時 */
    APP_EV_NET_STATUS,    /* 有線或 WiFi 連線狀態變了 */
} app_event_t;

/* 回傳下一個狀態。沒有定義的組合維持原狀態。 */
app_state_t app_fsm_next(app_state_t state, app_event_t event);

const char *app_fsm_state_name(app_state_t state);

#ifdef __cplusplus
}
#endif
