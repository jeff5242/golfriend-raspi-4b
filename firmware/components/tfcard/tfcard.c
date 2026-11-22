#include "tfcard.h"

#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "sdmmc_cmd.h"

static const char *TAG = "tfcard";

/*
 * 腳位來自廠商 board_pins.h，也是 ESP32-P4 SDMMC slot 0 的固定腳位。
 * WiFi 協處理器（esp_hosted）用 slot 1；兩個 slot 共用同一個 SDMMC 主機，
 * 掛載失敗時只釋放 slot 0（SDMMC_HOST_DEFAULT 的 deinit_p 是 sdmmc_host_deinit_slot）。
 */
#define TFCARD_SLOT     SDMMC_HOST_SLOT_0
#define TFCARD_CLK_GPIO 43
#define TFCARD_CMD_GPIO 44
#define TFCARD_D0_GPIO  39
#define TFCARD_D1_GPIO  40
#define TFCARD_D2_GPIO  41
#define TFCARD_D3_GPIO  42
/*
 * 卡片 IO 電源：晶片內建 LDO 通道 4（ESP32-P4 的標準接法）。
 * 廠商 board_pins.h 寫通道 3，但通道 3 已確認是螢幕 MIPI PHY 的 2.5 V，兩者不可能共用；
 * 出廠程式有呼叫 sd_pwr_ctrl_new_on_chip_ldo。通道 4 是推論，第一次插卡時驗證。
 */
#define TFCARD_LDO_CHAN 4
#define TFCARD_MAX_FILES 2

static sdmmc_card_t *s_card;
static sd_pwr_ctrl_handle_t s_pwr;

esp_err_t tfcard_mount(void)
{
    if (s_card != NULL) {
        return ESP_OK;
    }
    const sd_pwr_ctrl_ldo_config_t ldo_cfg = {.ldo_chan_id = TFCARD_LDO_CHAN};
    esp_err_t err = sd_pwr_ctrl_new_on_chip_ldo(&ldo_cfg, &s_pwr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "card power: %s", esp_err_to_name(err));
        return err;
    }

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = TFCARD_SLOT;
    host.pwr_ctrl_handle = s_pwr;

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = TFCARD_CLK_GPIO;
    slot.cmd = TFCARD_CMD_GPIO;
    slot.d0 = TFCARD_D0_GPIO;
    slot.d1 = TFCARD_D1_GPIO;
    slot.d2 = TFCARD_D2_GPIO;
    slot.d3 = TFCARD_D3_GPIO;

    const esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,  /* 不得格式化使用者的卡 */
        .max_files = TFCARD_MAX_FILES,
    };
    err = esp_vfs_fat_sdmmc_mount(TFCARD_MOUNT_POINT, &host, &slot, &mount_cfg, &s_card);
    if (err != ESP_OK) {
        /* 沒插卡是最常見的情況，不算錯誤 */
        ESP_LOGI(TAG, "no usable card (%s)", esp_err_to_name(err));
        s_card = NULL;
        sd_pwr_ctrl_del_on_chip_ldo(s_pwr);
        s_pwr = NULL;
        return err;
    }
    ESP_LOGI(TAG, "mounted, %llu MB", (unsigned long long)s_card->csd.capacity *
                                          s_card->csd.sector_size / (1024 * 1024));
    return ESP_OK;
}

void tfcard_unmount(void)
{
    if (s_card == NULL) {
        return;
    }
    const esp_err_t err = esp_vfs_fat_sdcard_unmount(TFCARD_MOUNT_POINT, s_card);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "unmount: %s", esp_err_to_name(err));
    }
    s_card = NULL;
    sd_pwr_ctrl_del_on_chip_ldo(s_pwr);
    s_pwr = NULL;
}
