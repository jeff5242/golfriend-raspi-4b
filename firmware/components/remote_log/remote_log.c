#include "remote_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "remote_log";

#define ENDPOINT          "https://in.logs.betterstack.com"  /* 同 Pi 版 */
#define FLUSH_INTERVAL_MS 5000   /* 同 Pi 版 FLUSH_INTERVAL */
#define BATCH_SIZE        20     /* 同 Pi 版 BATCH_SIZE */
#define QUEUE_LEN         48
#define MESSAGE_MAX       200
#define ENTRY_JSON_MAX    512
#define HTTP_TIMEOUT_MS   10000
#define TASK_STACK        8192
#define TOKEN_MAX         64
#define DEVICE_MAX        48
#define VALID_YEAR        2024
#define RETRY_MS          5000
#define SEND_ATTEMPTS     6      /* 最多等約 30 秒 */

typedef struct {
    time_t when;
    remote_log_tag_t tag;
    char level[8];
    char message[MESSAGE_MAX];
} item_t;

static QueueHandle_t s_queue;
static char s_auth[TOKEN_MAX + 8];  /* "Bearer <token>" */
static char s_device[DEVICE_MAX];

static bool clock_valid(time_t t)
{
    struct tm tm_utc;
    gmtime_r(&t, &tm_utc);
    return tm_utc.tm_year + 1900 >= VALID_YEAR;
}

/* 把 batch 寫成 JSON 陣列。回傳長度；放不下的項目略過。 */
static size_t build_body(char *body, size_t size, const item_t *batch, int count)
{
    size_t len = 0;
    body[len++] = '[';
    const time_t now = time(NULL);
    for (int i = 0; i < count; i++) {
        /* 對時之前發生的事件（開機那幾筆）用送出時的時間 */
        const time_t when = clock_valid(batch[i].when) ? batch[i].when : now;
        struct tm local;
        char dt[32];
        localtime_r(&when, &local);
        strftime(dt, sizeof dt, "%Y-%m-%dT%H:%M:%S%z", &local);
        const remote_log_entry_t entry = {dt, batch[i].level, batch[i].tag, s_device,
                                          batch[i].message};
        if (len + ENTRY_JSON_MAX + 2 > size) {
            break;
        }
        if (i > 0) {
            body[len++] = ',';
        }
        const int n = remote_log_format_entry(body + len, size - len - 2, &entry);
        if (n < 0) {
            len -= i > 0 ? 1 : 0;  /* 拿掉剛加的逗號 */
            continue;
        }
        len += (size_t)n;
    }
    body[len++] = ']';
    body[len] = '\0';
    return len;
}

static esp_err_t send_body(const char *body, size_t len)
{
    const esp_http_client_config_t cfg = {
        .url = ENDPOINT,
        .method = HTTP_METHOD_POST,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_http_client_set_header(client, "Authorization", s_auth);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, (int)len);
    esp_err_t err = esp_http_client_perform(client);
    const int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (err == ESP_OK && (status < 200 || status >= 300)) {
        ESP_LOGW(TAG, "server returned %d", status);
        err = ESP_ERR_INVALID_RESPONSE;
    }
    return err;
}

static void sender_task(void *arg)
{
    (void)arg;
    item_t *batch = malloc(sizeof(item_t) * BATCH_SIZE);
    const size_t body_size = (size_t)BATCH_SIZE * ENTRY_JSON_MAX + 4;
    char *body = malloc(body_size);
    if (batch == NULL || body == NULL) {
        ESP_LOGE(TAG, "no memory, remote log disabled");
        vTaskDelete(NULL);
    }
    for (;;) {
        /* 沒對時就先不送：事件留在佇列裡 */
        if (!clock_valid(time(NULL))) {
            vTaskDelay(pdMS_TO_TICKS(RETRY_MS));
            continue;
        }
        int count = 0;
        const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(FLUSH_INTERVAL_MS);
        while (count < BATCH_SIZE) {
            const TickType_t now = xTaskGetTickCount();
            const TickType_t wait = (int32_t)(deadline - now) > 0 ? deadline - now : 0;
            if (xQueueReceive(s_queue, &batch[count], wait) != pdTRUE) {
                break;
            }
            count++;
        }
        if (count == 0) {
            continue;
        }
        const size_t len = build_body(body, body_size, batch, count);
        /* 開機時網路還沒連上、或短暫斷線：同一批重試幾次。超過就丟掉，避免長時間斷網時累積 */
        esp_err_t err = send_body(body, len);
        for (int attempt = 1; err != ESP_OK && attempt < SEND_ATTEMPTS; attempt++) {
            vTaskDelay(pdMS_TO_TICKS(RETRY_MS));
            err = send_body(body, len);
        }
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "dropped %d entries: %s", count, esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "sent %d entries", count);
        }
    }
}

esp_err_t remote_log_start(const char *token, const char *device)
{
    if (token == NULL || token[0] == '\0') {
        ESP_LOGI(TAG, "no token, remote log off");
        return ESP_OK;
    }
    if (strlen(token) >= TOKEN_MAX || device == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    snprintf(s_auth, sizeof s_auth, "Bearer %s", token);
    snprintf(s_device, sizeof s_device, "%s", device);
    s_queue = xQueueCreate(QUEUE_LEN, sizeof(item_t));
    if (s_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(sender_task, "remote_log", TASK_STACK, NULL, 2, NULL) != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "remote log on, device=%s", s_device);
    return ESP_OK;
}

void remote_log_event(remote_log_tag_t tag, const char *level, const char *fmt, ...)
{
    if (s_queue == NULL || level == NULL || fmt == NULL) {
        return;
    }
    item_t item = {.when = time(NULL), .tag = tag};
    snprintf(item.level, sizeof item.level, "%s", level);
    va_list args;
    va_start(args, fmt);
    vsnprintf(item.message, sizeof item.message, fmt, args);
    va_end(args);
    xQueueSend(s_queue, &item, 0);  /* 滿了就丟掉這一筆 */
}
