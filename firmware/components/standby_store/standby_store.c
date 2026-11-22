#include "standby_store.h"

#include <stdio.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_partition.h"
#include "esp_rom_crc.h"
#include "standby_header.h"

static const char *TAG = "standby_store";

#define STANDBY_PARTITION_SUBTYPE 0x40  /* 自訂 data 子類型，見 partitions.csv */
#define STANDBY_PARTITION_LABEL   "standby"
#define FLASH_SECTOR_SIZE         4096
#define COPY_CHUNK                4096

static const esp_partition_t *find_partition(void)
{
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, STANDBY_PARTITION_SUBTYPE, STANDBY_PARTITION_LABEL);
    if (part == NULL) {
        ESP_LOGE(TAG, "partition \"%s\" not found (flash the new partition table)",
                 STANDBY_PARTITION_LABEL);
    }
    return part;
}

/* 讀整個檔案算 CRC32 與大小；讀完後檔案位置回到開頭 */
static esp_err_t measure_file(FILE *f, uint8_t *chunk, uint32_t *size, uint32_t *crc)
{
    uint32_t total = 0;
    uint32_t sum = 0;
    size_t n;
    while ((n = fread(chunk, 1, COPY_CHUNK, f)) > 0) {
        sum = esp_rom_crc32_le(sum, chunk, n);
        total += n;
    }
    if (ferror(f) || fseek(f, 0, SEEK_SET) != 0) {
        return ESP_FAIL;
    }
    *size = total;
    *crc = sum;
    return ESP_OK;
}

static esp_err_t write_image(const esp_partition_t *part, FILE *f, uint8_t *chunk,
                             const standby_header_t *header)
{
    const size_t used = sizeof *header + header->size;
    const size_t erase = (used + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE * FLASH_SECTOR_SIZE;
    esp_err_t err = esp_partition_erase_range(part, 0, erase);
    size_t offset = sizeof *header;
    size_t n;
    while (err == ESP_OK && (n = fread(chunk, 1, COPY_CHUNK, f)) > 0) {
        err = esp_partition_write(part, offset, chunk, n);
        offset += n;
    }
    if (err == ESP_OK && offset != used) {
        err = ESP_FAIL;  /* 檔案在複製途中變短（卡片讀取錯誤） */
    }
    /* 檔頭最後寫：中途斷電時檔頭仍是空白，下次開機當成沒有圖 */
    if (err == ESP_OK) {
        err = esp_partition_write(part, 0, header, sizeof *header);
    }
    return err;
}

esp_err_t standby_store_import_file(const char *path, bool *changed)
{
    if (path == NULL || changed == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *changed = false;
    const esp_partition_t *part = find_partition();
    if (part == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        ESP_LOGI(TAG, "%s not found, keeping current image", path);
        return ESP_ERR_NOT_FOUND;
    }
    uint8_t *chunk = malloc(COPY_CHUNK);
    esp_err_t err = chunk != NULL ? ESP_OK : ESP_ERR_NO_MEM;
    standby_header_t header = {.magic = STANDBY_HEADER_MAGIC};
    standby_header_t stored;
    if (err == ESP_OK) {
        err = measure_file(f, chunk, &header.size, &header.crc32);
    }
    if (err == ESP_OK && !standby_header_valid(&header, part->size)) {
        ESP_LOGE(TAG, "%s is %u bytes, limit is %u", path, (unsigned)header.size,
                 (unsigned)standby_max_image_size(part->size));
        err = ESP_ERR_INVALID_SIZE;
    }
    if (err == ESP_OK) {
        err = esp_partition_read(part, 0, &stored, sizeof stored);
    }
    if (err == ESP_OK && standby_header_matches(&stored, header.size, header.crc32)) {
        ESP_LOGI(TAG, "%s is the same as the stored image, not rewriting", path);
    } else if (err == ESP_OK) {
        err = write_image(part, f, chunk, &header);
        *changed = err == ESP_OK;
        ESP_LOGI(TAG, "%s -> flash: %u bytes (%s)", path, (unsigned)header.size,
                 esp_err_to_name(err));
    }
    free(chunk);
    fclose(f);
    return err;
}

esp_err_t standby_store_get(const uint8_t **jpg, uint32_t *size)
{
    if (jpg == NULL || size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const esp_partition_t *part = find_partition();
    if (part == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    standby_header_t header;
    esp_err_t err = esp_partition_read(part, 0, &header, sizeof header);
    if (err != ESP_OK) {
        return err;
    }
    if (!standby_header_valid(&header, part->size)) {
        return ESP_ERR_NOT_FOUND;  /* 還沒匯入過 */
    }
    const void *mapped = NULL;
    esp_partition_mmap_handle_t handle;  /* 不解除映射：圖片整個執行期間都要用 */
    err = esp_partition_mmap(part, 0, sizeof header + header.size, ESP_PARTITION_MMAP_DATA,
                             &mapped, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mmap: %s", esp_err_to_name(err));
        return err;
    }
    const uint8_t *data = (const uint8_t *)mapped + sizeof header;
    if (esp_rom_crc32_le(0, data, header.size) != header.crc32) {
        ESP_LOGE(TAG, "stored image is corrupted (crc mismatch)");
        esp_partition_munmap(handle);
        return ESP_ERR_INVALID_CRC;
    }
    *jpg = data;
    *size = header.size;
    return ESP_OK;
}
