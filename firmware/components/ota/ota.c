#include "ota.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "ota_policy.h"
#include "sdkconfig.h"

static const char *TAG = "ota";

#define MANIFEST_MAX    1024
#define HTTP_TIMEOUT_MS 15000

#ifdef CONFIG_GOLF_OTA_ALLOW_HTTP
#define OTA_ALLOW_HTTP true   /* 只給開發用：從區域網路的電腦下載 */
#else
#define OTA_ALLOW_HTTP false
#endif

/*
 * version 是不是上次更新後開機失敗、被 bootloader 退回的那一版。
 * 是的話不再下載：否則每次開機都會重新下載同一個壞版本，再退回，沒完沒了。
 * 發布修正版時必須用新的版本號。
 */
static bool is_known_bad(const char *version)
{
    const esp_partition_t *bad = esp_ota_get_last_invalid_partition();
    esp_app_desc_t desc;
    return bad != NULL && esp_ota_get_partition_description(bad, &desc) == ESP_OK &&
           strcmp(desc.version, version) == 0;
}

/* 讀 manifest 本文到 out（以 0 結尾） */
static esp_err_t fetch_manifest(const char *manifest_url, char *out, size_t out_size)
{
    const esp_http_client_config_t cfg = {
        .url = manifest_url,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = esp_http_client_open(client, 0);
    if (err == ESP_OK && esp_http_client_fetch_headers(client) < 0) {
        err = ESP_ERR_HTTP_FETCH_HEADER;
    }
    if (err == ESP_OK && esp_http_client_get_status_code(client) != 200) {
        ESP_LOGW(TAG, "manifest: HTTP %d", esp_http_client_get_status_code(client));
        err = ESP_ERR_INVALID_RESPONSE;
    }
    if (err == ESP_OK) {
        int total = 0;
        int n;
        while (total < (int)out_size - 1 &&
               (n = esp_http_client_read(client, out + total, (int)out_size - 1 - total)) > 0) {
            total += n;
        }
        out[total] = '\0';
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return err;
}

esp_err_t ota_check(const char *manifest_url, char *url, size_t url_size, char *version,
                    size_t version_size)
{
    if (manifest_url == NULL || url == NULL || version == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!ota_url_allowed(manifest_url, OTA_ALLOW_HTTP)) {
        ESP_LOGE(TAG, "manifest url must be https");
        return ESP_ERR_NOT_ALLOWED;
    }
    char *text = malloc(MANIFEST_MAX);
    if (text == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = fetch_manifest(manifest_url, text, MANIFEST_MAX);
    cJSON *root = err == ESP_OK ? cJSON_Parse(text) : NULL;
    free(text);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "manifest: %s", esp_err_to_name(err));
        return err;
    }
    const cJSON *j_version = cJSON_GetObjectItemCaseSensitive(root, "version");
    const cJSON *j_url = cJSON_GetObjectItemCaseSensitive(root, "url");
    const char *current = esp_app_get_description()->version;
    if (!cJSON_IsString(j_version) || !cJSON_IsString(j_url)) {
        ESP_LOGE(TAG, "manifest needs \"version\" and \"url\"");
        err = ESP_ERR_INVALID_RESPONSE;
    } else if (!ota_is_newer(current, j_version->valuestring)) {
        ESP_LOGI(TAG, "running %s, manifest has %s: no update", current, j_version->valuestring);
        err = ESP_ERR_NOT_FOUND;
    } else if (is_known_bad(j_version->valuestring)) {
        ESP_LOGW(TAG, "%s failed to start before and was rolled back: not downloading again",
                 j_version->valuestring);
        err = ESP_ERR_NOT_FOUND;
    } else if (!ota_url_allowed(j_url->valuestring, OTA_ALLOW_HTTP) ||
               strlen(j_url->valuestring) >= url_size ||
               strlen(j_version->valuestring) >= version_size) {
        ESP_LOGE(TAG, "firmware url not allowed or too long");
        err = ESP_ERR_NOT_ALLOWED;
    } else {
        snprintf(url, url_size, "%s", j_url->valuestring);
        snprintf(version, version_size, "%s", j_version->valuestring);
        ESP_LOGI(TAG, "update available: %s -> %s", current, version);
    }
    cJSON_Delete(root);
    return err;
}

esp_err_t ota_download(const char *url)
{
    if (!ota_url_allowed(url, OTA_ALLOW_HTTP)) {
        return ESP_ERR_NOT_ALLOWED;
    }
    const esp_http_client_config_t http_cfg = {
        .url = url,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
    };
    const esp_https_ota_config_t ota_cfg = {.http_config = &http_cfg};
    /* esp_https_ota 會檢查映像檔的晶片型號與檢查碼；寫入的是目前沒在用的那個 app 分割區 */
    const esp_err_t err = esp_https_ota(&ota_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "download failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "download ok, reboot to run the new version");
    }
    return err;
}

bool ota_is_pending_verify(void)
{
    esp_ota_img_states_t state;
    const esp_partition_t *running = esp_ota_get_running_partition();
    return esp_ota_get_state_partition(running, &state) == ESP_OK &&
           state == ESP_OTA_IMG_PENDING_VERIFY;
}

void ota_confirm_running(void)
{
    if (!ota_is_pending_verify()) {
        return;
    }
    const esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    ESP_LOGI(TAG, "new version confirmed (%s)", esp_err_to_name(err));
}
