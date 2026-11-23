#include "app_fsm.h"

app_state_t app_fsm_next(app_state_t state, app_event_t event)
{
    switch (state) {
    case APP_ST_STANDBY:
        if (event == APP_EV_KEY || event == APP_EV_TOUCH) return APP_ST_INPUT;
        if (event == APP_EV_LINE) return APP_ST_QUERY;
        if (event == APP_EV_MAINT) return APP_ST_MENU;
        if (event == APP_EV_UPDATE_START) return APP_ST_UPDATE;
        if (event == APP_EV_SCAN) return APP_ST_SCANNED;
        if (event == APP_EV_WIFI_OPEN) return APP_ST_WIFI;
        break;
    case APP_ST_INPUT:
        if (event == APP_EV_LINE) return APP_ST_QUERY;
        if (event == APP_EV_TIMEOUT) return APP_ST_STANDBY;
        if (event == APP_EV_MAINT) return APP_ST_MENU;
        if (event == APP_EV_SCAN) return APP_ST_SCANNED;
        break;
    case APP_ST_SCANNED:
        if (event == APP_EV_TIMEOUT || event == APP_EV_LINE) return APP_ST_QUERY;
        if (event == APP_EV_KEY) return APP_ST_INPUT;
        if (event == APP_EV_MAINT) return APP_ST_MENU;
        break;
    case APP_ST_QUERY:
        if (event == APP_EV_API_OK) return APP_ST_CONFIRM;
        if (event == APP_EV_API_FAIL || event == APP_EV_TIMEOUT) return APP_ST_ERROR;
        break;
    case APP_ST_CONFIRM:
        if (event == APP_EV_CONFIRM_OK) return APP_ST_DISPENSE;
        if (event == APP_EV_API_FAIL || event == APP_EV_TIMEOUT) return APP_ST_ERROR;
        break;
    case APP_ST_DISPENSE:
        if (event == APP_EV_MOTOR_DONE) return APP_ST_STANDBY;
        if (event == APP_EV_DISPENSE_FAIL) return APP_ST_ERROR;
        break;
    case APP_ST_ERROR:
        if (event == APP_EV_ERROR_SHOWN) return APP_ST_STANDBY;
        break;
    case APP_ST_MENU:
        if (event == APP_EV_MENU_CLOSE || event == APP_EV_TIMEOUT) return APP_ST_STANDBY;
        break;
    case APP_ST_UPDATE:
        if (event == APP_EV_UPDATE_END) return APP_ST_STANDBY;
        break;
    case APP_ST_WIFI:
        /* 掃碼、維修碼、韌體更新都不收：設定到一半不得被打斷 */
        if (event == APP_EV_WIFI_CLOSE || event == APP_EV_TIMEOUT) return APP_ST_STANDBY;
        break;
    }
    return state;
}

const char *app_fsm_state_name(app_state_t state)
{
    switch (state) {
    case APP_ST_STANDBY:  return "STANDBY";
    case APP_ST_INPUT:    return "INPUT";
    case APP_ST_QUERY:    return "QUERY";
    case APP_ST_CONFIRM:  return "CONFIRM";
    case APP_ST_DISPENSE: return "DISPENSE";
    case APP_ST_ERROR:    return "ERROR";
    case APP_ST_MENU:     return "MENU";
    case APP_ST_UPDATE:   return "UPDATE";
    case APP_ST_SCANNED:  return "SCANNED";
    case APP_ST_WIFI:     return "WIFI";
    }
    return "UNKNOWN";
}
