#include "api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "api_sign.h"
#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "mbedtls/sha1.h"

static const char *TAG = "api";

#define API_TIMEOUT_MS     10000  /* 同 Pi 版 requests timeout=10 */
#define API_URL_MAX        160
#define API_BODY_MAX       512
#define API_RESP_MAX       2048
#define API_SIGN_INPUT_MAX 256
#define API_VALID_YEAR     2024

static char s_base_url[96];
static char s_device_code[48];

esp_err_t api_init(const char *base_url, const char *device_code)
{
    if (base_url == NULL || device_code == NULL || device_code[0] == '\0') {
        ESP_LOGE(TAG, "device code not set (menuconfig: GOLF_DEVICE_CODE)");
        return ESP_ERR_INVALID_ARG;
    }
    snprintf(s_base_url, sizeof s_base_url, "%s", base_url);
    snprintf(s_device_code, sizeof s_device_code, "%s", device_code);
    return ESP_OK;
}

/* SNTP 對時完成時呼叫（在 lwIP 工作中執行，只記 log） */
static void on_time_synced(struct timeval *tv)
{
    struct tm tm_utc;
    char text[24];
    gmtime_r(&tv->tv_sec, &tm_utc);
    strftime(text, sizeof text, "%Y-%m-%d %H:%M:%S", &tm_utc);
    ESP_LOGI(TAG, "time synced: %s UTC", text);
}

esp_err_t api_time_sync_start(void)
{
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    cfg.sync_cb = on_time_synced;
    const esp_err_t err = esp_netif_sntp_init(&cfg);
    if (err != ESP_OK) {
        /* 沒對時，簽章時間戳會錯，伺服器拒絕所有交易 */
        ESP_LOGE(TAG, "esp_netif_sntp_init: %s", esp_err_to_name(err));
    }
    return err;
}

bool api_time_is_valid(void)
{
    time_t now = time(NULL);
    struct tm tm_now;
    gmtime_r(&now, &tm_now);
    return tm_now.tm_year + 1900 >= API_VALID_YEAR;
}

/* sha1(input) 轉成 40 碼小寫十六進位，同 Python hexdigest() */
static esp_err_t sha1_hex(const char *input, char out[41])
{
    unsigned char digest[20];
    if (mbedtls_sha1((const unsigned char *)input, strlen(input), digest) != 0) {
        return ESP_FAIL;
    }
    for (int i = 0; i < 20; i++) {
        snprintf(out + i * 2, 3, "%02x", digest[i]);
    }
    return ESP_OK;
}

/*
 * POST 表單，回應本文寫到 resp。後台用 200 和 400 都回 JSON（同 Pi 版判斷）。
 * 不得回傳 ESP_FAIL：ESP_FAIL 只代表「後台回 error=true」，畫面依它決定要不要查錯誤代碼。
 */
static esp_err_t post_form(const char *path, const char *body, char *resp, size_t resp_size)
{
    char url[API_URL_MAX];
    snprintf(url, sizeof url, "%s/%s", s_base_url, path);

    const esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = API_TIMEOUT_MS,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");

    const int body_len = (int)strlen(body);
    esp_err_t err = esp_http_client_open(client, body_len);
    if (err == ESP_OK && esp_http_client_write(client, body, body_len) != body_len) {
        err = ESP_ERR_HTTP_WRITE_DATA;
    }
    if (err == ESP_OK && esp_http_client_fetch_headers(client) < 0) {
        err = ESP_ERR_HTTP_FETCH_HEADER;
    }
    if (err == ESP_OK) {
        const int status = esp_http_client_get_status_code(client);
        int total = 0;
        int n;
        while (total < (int)resp_size - 1 &&
               (n = esp_http_client_read(client, resp + total, (int)resp_size - 1 - total)) > 0) {
            total += n;
        }
        resp[total] = '\0';
        ESP_LOGI(TAG, "POST %s -> %d, %d bytes", path, status, total);
        if (status != 200 && status != 400) {
            err = ESP_ERR_INVALID_RESPONSE;
        }
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (err == ESP_FAIL) {
        err = ESP_ERR_INVALID_RESPONSE;  /* 驅動內部回的 ESP_FAIL 也算網路錯誤 */
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "POST %s failed: %s", path, esp_err_to_name(err));
    }
    return err;
}

/* 後台欄位可能是數字或數字字串（Pi 版用 float()/int() 轉），兩種都接受 */
static double json_number(const cJSON *obj, const char *key, double fallback)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(item)) {
        return item->valuedouble;
    }
    if (cJSON_IsString(item) && item->valuestring != NULL) {
        return strtod(item->valuestring, NULL);
    }
    return fallback;
}

static void json_copy_text(const cJSON *obj, const char *key, char *out, size_t out_size)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(item) && item->valuestring != NULL) {
        snprintf(out, out_size, "%s", item->valuestring);
    } else if (cJSON_IsNumber(item)) {
        snprintf(out, out_size, "%.0f", item->valuedouble);
    } else {
        out[0] = '\0';
    }
}

/* 解析 {"error": bool, "error_msg": ..., "body": {...}}。body_out 可為 NULL。 */
static esp_err_t parse_envelope(const char *resp, cJSON **root_out, const cJSON **body_out,
                                char *err_msg, size_t err_size)
{
    cJSON *root = cJSON_Parse(resp);
    if (root == NULL) {
        snprintf(err_msg, err_size, "bad json");
        return ESP_ERR_INVALID_RESPONSE;
    }
    *root_out = root;
    const cJSON *error = cJSON_GetObjectItemCaseSensitive(root, "error");
    if (!cJSON_IsFalse(error)) {
        json_copy_text(root, "error_msg", err_msg, err_size);
        return ESP_FAIL;
    }
    if (body_out != NULL) {
        *body_out = cJSON_GetObjectItemCaseSensitive(root, "body");
        if (!cJSON_IsObject(*body_out)) {
            snprintf(err_msg, err_size, "no body");
            return ESP_ERR_INVALID_RESPONSE;
        }
    }
    return ESP_OK;
}

static esp_err_t make_time_str(char *out, size_t out_size)
{
    if (!api_time_is_valid()) {
        ESP_LOGE(TAG, "clock not synced, refuse to sign");
        return ESP_ERR_INVALID_STATE;
    }
    snprintf(out, out_size, "%lld", (long long)time(NULL));
    return ESP_OK;
}

esp_err_t api_take_ball(const char *transaction_code, api_take_ball_result_t *out,
                        char *err_msg, size_t err_size)
{
    if (transaction_code == NULL || out == NULL || err_msg == NULL || err_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    err_msg[0] = '\0';

    char time_str[24];
    char sign_input[API_SIGN_INPUT_MAX];
    char sn[41];
    esp_err_t err = make_time_str(time_str, sizeof time_str);
    if (err != ESP_OK) {
        return err;
    }
    if (api_sign_input(sign_input, sizeof sign_input, time_str, transaction_code,
                       s_device_code) < 0 ||
        sha1_hex(sign_input, sn) != ESP_OK) {
        return ESP_ERR_INVALID_SIZE;
    }

    const char *keys[] = {"device_code", "transaction_code", "time", "sn"};
    const char *vals[] = {s_device_code, transaction_code, time_str, sn};
    char body[API_BODY_MAX];
    if (api_form_encode(body, sizeof body, keys, vals, 4) < 0) {
        return ESP_ERR_INVALID_SIZE;
    }

    char *resp = malloc(API_RESP_MAX);
    if (resp == NULL) {
        return ESP_ERR_NO_MEM;
    }
    err = post_form("take_ball", body, resp, API_RESP_MAX);
    if (err == ESP_OK) {
        cJSON *root = NULL;
        const cJSON *b = NULL;
        err = parse_envelope(resp, &root, &b, err_msg, err_size);
        if (err == ESP_OK) {
            json_copy_text(b, "id", out->id, sizeof out->id);
            out->take = (int)json_number(b, "take", 0);
            out->type = (int)json_number(b, "type", 0);
            out->timer_s = (float)json_number(b, "timer", 0);
            out->start_timer_s = (float)json_number(b, "start_timer", 0);
            out->interval_timer_s = (float)json_number(b, "interval_timer", 0);
            out->ball_per_plate = (float)json_number(b, "ball_per_plate", 0);
            out->point_before = (long)json_number(b, "point1", API_POINT_HIDDEN);
            out->point_after = (long)json_number(b, "point2", API_POINT_HIDDEN);
        }
        cJSON_Delete(root);
    }
    free(resp);
    return err;
}

esp_err_t api_take_ball_confirm(const char *response_id, const char *note, const char *status,
                                char *err_msg, size_t err_size)
{
    if (response_id == NULL || note == NULL || status == NULL || err_msg == NULL ||
        err_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    err_msg[0] = '\0';

    char time_str[24];
    char sign_input[API_SIGN_INPUT_MAX];
    char sn[41];
    esp_err_t err = make_time_str(time_str, sizeof time_str);
    if (err != ESP_OK) {
        return err;
    }
    if (api_sign_input(sign_input, sizeof sign_input, time_str, response_id, NULL) < 0 ||
        sha1_hex(sign_input, sn) != ESP_OK) {
        return ESP_ERR_INVALID_SIZE;
    }

    const char *keys[] = {"transaction_code", "time", "sn", "note", "status"};
    const char *vals[] = {response_id, time_str, sn, note, status};
    char body[API_BODY_MAX];
    if (api_form_encode(body, sizeof body, keys, vals, 5) < 0) {
        return ESP_ERR_INVALID_SIZE;
    }

    char *resp = malloc(API_RESP_MAX);
    if (resp == NULL) {
        return ESP_ERR_NO_MEM;
    }
    err = post_form("take_ball_confirm", body, resp, API_RESP_MAX);
    if (err == ESP_OK) {
        cJSON *root = NULL;
        err = parse_envelope(resp, &root, NULL, err_msg, err_size);
        cJSON_Delete(root);
    }
    free(resp);
    return err;
}

esp_err_t api_ping(const char *note, char *err_msg, size_t err_size)
{
    if (note == NULL || err_msg == NULL || err_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    err_msg[0] = '\0';

    char time_str[24];
    char sign_input[API_SIGN_INPUT_MAX];
    char sn[41];
    esp_err_t err = make_time_str(time_str, sizeof time_str);
    if (err != ESP_OK) {
        return err;
    }
    /* 同 Pi 版 SendPingCmd：sha1("salt" + time + note + device_code) */
    if (api_sign_input(sign_input, sizeof sign_input, time_str, note, s_device_code) < 0 ||
        sha1_hex(sign_input, sn) != ESP_OK) {
        return ESP_ERR_INVALID_SIZE;
    }

    const char *keys[] = {"device_code", "time", "note", "sn"};
    const char *vals[] = {s_device_code, time_str, note, sn};
    char body[API_BODY_MAX];
    if (api_form_encode(body, sizeof body, keys, vals, 4) < 0) {
        return ESP_ERR_INVALID_SIZE;
    }

    char *resp = malloc(API_RESP_MAX);
    if (resp == NULL) {
        return ESP_ERR_NO_MEM;
    }
    err = post_form("take_ball_ping", body, resp, API_RESP_MAX);
    if (err == ESP_OK) {
        cJSON *root = NULL;
        err = parse_envelope(resp, &root, NULL, err_msg, err_size);
        cJSON_Delete(root);
    }
    free(resp);
    return err;
}
