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

typedef void (*motor_done_cb_t)(void *ctx);

/* 初始化 GPIO。腳位一開始就是 LOW，開機不會出球。gpio_num < 0 回傳 ESP_ERR_INVALID_ARG。 */
esp_err_t motor_init(int gpio_num);

/* 依時序出球。跑完後在 esp_timer 工作中呼叫 on_done。正在跑時回傳 ESP_ERR_INVALID_STATE。 */
esp_err_t motor_run(const motor_plan_t *plan, motor_done_cb_t on_done, void *ctx);

/* 立即停止並把腳位拉 LOW。不會呼叫 on_done。 */
void motor_stop(void);

bool motor_is_running(void);

#ifdef __cplusplus
}
#endif
