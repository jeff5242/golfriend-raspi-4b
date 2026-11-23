#include "settings.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "sdkconfig.h"

static const char *TAG = "settings";

#define SETTINGS_NAMESPACE "golf"
#define SETTINGS_FILE_MAX  2048  /* 設定檔上限；超過的部分不讀 */

typedef struct {
    const char *key;  /* NVS key，最長 15 字元 */
    size_t offset;
    size_t size;
} field_t;

#define FIELD(name) {#name, offsetof(settings_t, name), sizeof(((settings_t *)0)->name)}

static const field_t s_fields[] = {
    FIELD(device_code), FIELD(wifi1_ssid), FIELD(wifi1_password),
    FIELD(wifi2_ssid),  FIELD(wifi2_password), FIELD(log_token),
    FIELD(ota_url),
};
#define FIELD_COUNT (sizeof s_fields / sizeof s_fields[0])

static char *field_ptr(settings_t *s, const field_t *f)
{
    return (char *)s + f->offset;
}

static void load_defaults(settings_t *out)
{
    snprintf(out->device_code, sizeof out->device_code, "%s", CONFIG_GOLF_DEVICE_CODE);
    snprintf(out->wifi1_ssid, sizeof out->wifi1_ssid, "%s", CONFIG_GOLF_WIFI1_SSID);
    snprintf(out->wifi1_password, sizeof out->wifi1_password, "%s", CONFIG_GOLF_WIFI1_PASSWORD);
    snprintf(out->wifi2_ssid, sizeof out->wifi2_ssid, "%s", CONFIG_GOLF_WIFI2_SSID);
    snprintf(out->wifi2_password, sizeof out->wifi2_password, "%s", CONFIG_GOLF_WIFI2_PASSWORD);
    snprintf(out->log_token, sizeof out->log_token, "%s", CONFIG_GOLF_LOG_TOKEN);
    snprintf(out->ota_url, sizeof out->ota_url, "%s", CONFIG_GOLF_OTA_URL);
}

void settings_load(settings_t *out)
{
    load_defaults(out);
    nvs_handle_t nvs;
    const esp_err_t err = nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        /* 還沒匯入過設定檔時命名空間不存在，屬於正常情況 */
        ESP_LOGI(TAG, "no stored settings (%s), using build defaults", esp_err_to_name(err));
        return;
    }
    int stored = 0;
    for (size_t i = 0; i < FIELD_COUNT; i++) {
        size_t len = s_fields[i].size;
        if (nvs_get_str(nvs, s_fields[i].key, field_ptr(out, &s_fields[i]), &len) == ESP_OK) {
            stored++;
        }
    }
    nvs_close(nvs);
    ESP_LOGI(TAG, "%d of %d items from stored settings", stored, (int)FIELD_COUNT);
}

esp_err_t settings_save(const settings_t *settings)
{
    if (settings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open: %s", esp_err_to_name(err));
        return err;
    }
    for (size_t i = 0; i < FIELD_COUNT && err == ESP_OK; i++) {
        err = nvs_set_str(nvs, s_fields[i].key, (const char *)settings + s_fields[i].offset);
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
    }
    return err;
}

int settings_import_file(const char *path, settings_t *settings)
{
    if (path == NULL || settings == NULL) {
        return 0;
    }
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGI(TAG, "%s not found, keeping current settings", path);
        return 0;
    }
    static char text[SETTINGS_FILE_MAX + 1];
    const size_t n = fread(text, 1, SETTINGS_FILE_MAX, f);
    fclose(f);
    text[n] = '\0';

    settings_t updated = *settings;
    const settings_parse_result_t r = settings_parse(text, &updated);
    /* 不印內容：設定檔有 WiFi 密碼 */
    ESP_LOGI(TAG, "%s: %d applied, %d rejected", path, r.applied, r.rejected);
    if (r.applied == 0) {
        return 0;
    }
    if (memcmp(&updated, settings, sizeof updated) != 0 && settings_save(&updated) != ESP_OK) {
        return 0;  /* 存不進去就不套用，避免這次開機和下次開機的設定不同 */
    }
    *settings = updated;
    return r.applied;
}
