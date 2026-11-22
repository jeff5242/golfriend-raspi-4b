#pragma once
/*
 * 待機圖儲存區的檔頭（純邏輯，可以在主機端測試）。
 * 分割區內容：檔頭（12 bytes，小端序）＋ JPEG 資料。
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STANDBY_HEADER_MAGIC 0x42545347u  /* "GSTB" */

typedef struct {
    uint32_t magic;
    uint32_t size;   /* JPEG 的 bytes */
    uint32_t crc32;  /* JPEG 的 CRC32 */
} standby_header_t;

/* 分割區能放的最大 JPEG（扣掉檔頭）。分割區比檔頭小時回傳 0。 */
size_t standby_max_image_size(size_t partition_size);

/* 檔頭是否合理：magic 正確、size 大於 0 且放得進分割區。空白 Flash（全 0xFF）回傳 false。 */
bool standby_header_valid(const standby_header_t *header, size_t partition_size);

/* 儲存區裡的圖是否就是這張（大小與 CRC 相同）：相同就不必重寫 Flash。 */
bool standby_header_matches(const standby_header_t *header, uint32_t size, uint32_t crc32);

#ifdef __cplusplus
}
#endif
