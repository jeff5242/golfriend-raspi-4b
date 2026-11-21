/*
 * motor.c 的主機端測試：用 idf_stub/ 的替身取代 GPIO 與 esp_timer。
 * 替身可以讓 esp_timer_start_once 在第 N 次呼叫時失敗，並由測試手動觸發計時器。
 */
#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_timer.h"
#include "motor.h"
#include "test_util.h"

/* ---- 替身 -------------------------------------------------------------- */

struct stub_timer {
    esp_timer_cb_t cb;
    void *arg;
    bool armed;
};
static struct stub_timer g_timer;
static int g_level = -1;
static int g_start_calls;
static int g_fail_on_call;  /* 第幾次 esp_timer_start_once 失敗；0 = 不失敗 */

const char *esp_err_to_name(esp_err_t code)
{
    return code == ESP_OK ? "ESP_OK" : "ESP_ERR";
}
esp_err_t gpio_config(const gpio_config_t *cfg)
{
    (void)cfg;
    return ESP_OK;
}
esp_err_t gpio_set_level(int gpio_num, uint32_t level)
{
    (void)gpio_num;
    g_level = (int)level;
    return ESP_OK;
}
esp_err_t esp_timer_create(const esp_timer_create_args_t *args, esp_timer_handle_t *out)
{
    g_timer.cb = args->callback;
    g_timer.arg = args->arg;
    *out = &g_timer;
    return ESP_OK;
}
esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t timeout_us)
{
    (void)timeout_us;
    g_start_calls++;
    if (g_start_calls == g_fail_on_call) {
        return ESP_ERR_NO_MEM;
    }
    timer->armed = true;
    return ESP_OK;
}
esp_err_t esp_timer_stop(esp_timer_handle_t timer)
{
    timer->armed = false;
    return ESP_OK;
}
int64_t esp_timer_get_time(void)
{
    return 0;
}

static void fire_timer(void)
{
    g_timer.armed = false;
    g_timer.cb(g_timer.arg);
}

/* ---- 出球完成回呼 ------------------------------------------------------ */

static int g_done_calls;
static esp_err_t g_done_result;

static void on_done(esp_err_t result, void *ctx)
{
    (void)ctx;
    g_done_calls++;
    g_done_result = result;
}

static void reset_stub(int fail_on_call)
{
    g_start_calls = 0;
    g_fail_on_call = fail_on_call;
    g_done_calls = 0;
    g_done_result = ESP_OK;
}

static const motor_plan_t TWO_PLATES = {
    .mode = MOTOR_MODE_PLATE, .pulses = 2, .on_ms = 1000, .off_ms = 6000};

/* ---- 測試 -------------------------------------------------------------- */

static void test_normal_run_reports_ok(void)
{
    reset_stub(0);
    CHECK(motor_run(&TWO_PLATES, on_done, NULL) == ESP_OK);
    CHECK(g_level == 1);
    fire_timer();                       /* 第 1 盤通電結束 */
    CHECK(g_level == 0);
    fire_timer();                       /* 斷電結束，第 2 盤通電 */
    CHECK(g_level == 1);
    fire_timer();                       /* 第 2 盤通電結束 */
    CHECK(g_level == 0);
    CHECK(g_done_calls == 1);
    CHECK(g_done_result == ESP_OK);
    CHECK(!motor_is_running());
}

static void test_timer_fails_at_start(void)
{
    /* 審查 P2：計時器啟動失敗時，繼電器不得一直通電 */
    reset_stub(1);
    CHECK(motor_run(&TWO_PLATES, on_done, NULL) != ESP_OK);
    CHECK(g_level == 0);
    CHECK(!motor_is_running());
    CHECK(motor_is_ready());
    CHECK(g_done_calls == 0);           /* 回傳值已經報錯，不再呼叫 on_done */
}

static void test_timer_fails_mid_run(void)
{
    /* 第 1 盤結束後排程斷電段失敗：腳位拉 LOW，並回報失敗，主程式才不會卡在出球畫面 */
    reset_stub(2);
    CHECK(motor_run(&TWO_PLATES, on_done, NULL) == ESP_OK);
    fire_timer();
    CHECK(g_level == 0);
    CHECK(!motor_is_running());
    CHECK(g_done_calls == 1);
    CHECK(g_done_result != ESP_OK);
}

static void test_timer_fails_when_turning_on(void)
{
    /* 斷電段結束、第 2 盤通電後排程失敗：腳位必須回到 LOW */
    reset_stub(3);
    CHECK(motor_run(&TWO_PLATES, on_done, NULL) == ESP_OK);
    fire_timer();
    fire_timer();
    CHECK(g_level == 0);
    CHECK(!motor_is_running());
    CHECK(g_done_calls == 1);
    CHECK(g_done_result != ESP_OK);
}

static void test_rejects_unrunnable_plan(void)
{
    const motor_plan_t no_on = {.mode = MOTOR_MODE_TIMED, .pulses = 1, .on_ms = 0};
    reset_stub(0);
    CHECK(motor_run(&no_on, on_done, NULL) == ESP_ERR_INVALID_ARG);
    CHECK(g_level == 0);
}

int main(void)
{
    CHECK(motor_init(18) == ESP_OK);
    CHECK(g_level == 0);
    RUN(test_normal_run_reports_ok);
    RUN(test_timer_fails_at_start);
    RUN(test_timer_fails_mid_run);
    RUN(test_timer_fails_when_turning_on);
    RUN(test_rejects_unrunnable_plan);
    return TEST_SUMMARY();
}
