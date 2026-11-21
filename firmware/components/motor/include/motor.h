#pragma once
/*
 * 出球馬達控制：用一支 GPIO 驅動繼電器模組（HW-482）的 S 腳。
 * 不得把馬達直接接到 GPIO；GPIO 只有 3.3V、幾十 mA。
 */
#include <stdbool.h>

#include "esp_err.h"
#include "motor_plan.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 出球結束時呼叫。result 不是 ESP_OK 表示中途停止（腳位已拉 LOW）。 */
typedef void (*motor_done_cb_t)(esp_err_t result, void *ctx);

/* 初始化 GPIO。腳位一開始就是 LOW，開機不會出球。gpio_num < 0 回傳 ESP_ERR_INVALID_ARG。 */
esp_err_t motor_init(int gpio_num);

/*
 * 依時序出球。跑完或中途計時器失敗時，在 esp_timer 工作中呼叫 on_done。
 * 正在跑時回傳 ESP_ERR_INVALID_STATE。一開始就啟動失敗時回傳錯誤，不呼叫 on_done。
 */
esp_err_t motor_run(const motor_plan_t *plan, motor_done_cb_t on_done, void *ctx);

/* 立即停止並把腳位拉 LOW。不會呼叫 on_done。 */
void motor_stop(void);

bool motor_is_running(void);

/* 已初始化且沒在跑時回傳 true。送確認（扣點）前用來確認出得了球。 */
bool motor_is_ready(void);

#ifdef __cplusplus
}
#endif
