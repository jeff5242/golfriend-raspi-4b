#pragma once
/*
 * TF 卡（板子背面的卡座，SDMMC slot 0）。只在開機時掛載，用來匯入現場設定與待機圖。
 * 檔案系統必須是 FAT／FAT32／exFAT 以外不支援；不會格式化卡片。
 */
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TFCARD_MOUNT_POINT "/sdcard"

/* 掛載到 TFCARD_MOUNT_POINT。沒插卡或卡片無法讀取時回傳錯誤，不影響其他功能。 */
esp_err_t tfcard_mount(void);

/* 卸載。沒有掛載時不做事。卸載後可以拔卡。 */
void tfcard_unmount(void);

#ifdef __cplusplus
}
#endif
