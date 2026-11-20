#include "motor.h"

#include <stdint.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "motor";

static int s_gpio = -1;
static esp_timer_handle_t s_timer;
static motor_plan_t s_plan;
static uint32_t s_pulses_done;
static bool s_on;
static volatile bool s_running;
static motor_done_cb_t s_on_done;
static void *s_ctx;

static void set_output(bool on)
{
    s_on = on;
    gpio_set_level(s_gpio, on ? 1 : 0);
}

static void schedule_ms(uint32_t ms)
{
    esp_timer_start_once(s_timer, (uint64_t)(ms == 0 ? 1 : ms) * 1000ULL);
}

/* esp_timer 工作中執行：通電段結束 → 斷電；斷電段結束 → 下一盤通電 */
static void on_timer(void *arg)
{
    (void)arg;
    if (s_on) {
        set_output(false);
        s_pulses_done++;
        ESP_LOGI(TAG, "pulse %u/%u done", (unsigned)s_pulses_done, (unsigned)s_plan.pulses);
        if (s_pulses_done >= s_plan.pulses) {
            s_running = false;
            if (s_on_done != NULL) {
                s_on_done(s_ctx);
            }
            return;
        }
        schedule_ms(s_plan.off_ms);
        return;
    }
    set_output(true);
    schedule_ms(s_plan.on_ms);
}

esp_err_t motor_init(int gpio_num)
{
    if (gpio_num < 0) {
        ESP_LOGE(TAG, "motor GPIO not set (menuconfig: GOLF_MOTOR_GPIO)");
        return ESP_ERR_INVALID_ARG;
    }
    s_gpio = gpio_num;

    /*
     * 先把輸出暫存器寫成 LOW，再切成輸出模式，最後再寫一次 LOW。
     * 2026-10-25 鐵砧山 Pi 版開機自動出球，原因是腳位切成輸出時沿用了當下的 HIGH。
     */
    gpio_set_level(s_gpio, 0);
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << s_gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        return err;
    }
    set_output(false);

    const esp_timer_create_args_t args = {
        .callback = on_timer,
        .name = "motor",
    };
    err = esp_timer_create(&args, &s_timer);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "motor ready on GPIO%d (LOW)", s_gpio);
    }
    return err;
}

esp_err_t motor_run(const motor_plan_t *plan, motor_done_cb_t on_done, void *ctx)
{
    if (plan == NULL || plan->pulses == 0 || plan->on_ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_gpio < 0 || s_timer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_running) {
        return ESP_ERR_INVALID_STATE;
    }
    s_plan = *plan;
    s_pulses_done = 0;
    s_on_done = on_done;
    s_ctx = ctx;
    s_running = true;
    ESP_LOGI(TAG, "run: mode=%d pulses=%u on=%ums off=%ums", (int)plan->mode,
             (unsigned)plan->pulses, (unsigned)plan->on_ms, (unsigned)plan->off_ms);
    set_output(true);
    schedule_ms(s_plan.on_ms);
    return ESP_OK;
}

void motor_stop(void)
{
    if (s_timer != NULL) {
        esp_timer_stop(s_timer);
    }
    if (s_gpio >= 0) {
        set_output(false);
    }
    s_running = false;
}

bool motor_is_running(void)
{
    return s_running;
}
