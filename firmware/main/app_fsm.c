#include "app_fsm.h"

app_state_t app_fsm_next(app_state_t state, app_event_t event)
{
    switch (state) {
    case APP_ST_STANDBY:
        if (event == APP_EV_KEY) return APP_ST_INPUT;
        if (event == APP_EV_LINE) return APP_ST_QUERY;
        break;
    case APP_ST_INPUT:
        if (event == APP_EV_LINE) return APP_ST_QUERY;
        if (event == APP_EV_TIMEOUT) return APP_ST_STANDBY;
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
        break;
    case APP_ST_ERROR:
        if (event == APP_EV_ERROR_SHOWN) return APP_ST_STANDBY;
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
    }
    return "UNKNOWN";
}
