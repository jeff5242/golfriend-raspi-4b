#include "ui_image.h"

#include <string.h>

#include "driver/jpeg_decode.h"
#include "esp_check.h"
#include "esp_log.h"
#include "standby_store.h"
#include "ui_board.h"

static const char *TAG = "ui_image";

#define JPEG_BLOCK_PX       16     /* 硬體解碼的輸出寬高會進位到 16 的倍數 */
#define JPEG_TIMEOUT_MS     2000
#define RGB565_BYTES_PER_PX 2

extern const uint8_t standby_jpg_start[] asm("_binary_standby_jpg_start");
extern const uint8_t standby_jpg_end[] asm("_binary_standby_jpg_end");

static lv_image_dsc_t s_standby;

static uint32_t round_up_block(uint32_t px)
{
    return (px + JPEG_BLOCK_PX - 1) / JPEG_BLOCK_PX * JPEG_BLOCK_PX;
}

/* 解碼到 *pixels（由這個函式配置；失敗時已釋放） */
static esp_err_t decode(const uint8_t *jpg, uint32_t jpg_size, uint8_t **pixels, uint32_t *stride)
{
    jpeg_decode_picture_info_t info;
    ESP_RETURN_ON_ERROR(jpeg_decoder_get_info(jpg, jpg_size, &info), TAG, "jpeg header");
    ESP_RETURN_ON_FALSE(info.width == UI_LCD_H_RES && info.height == UI_LCD_V_RES,
                        ESP_ERR_INVALID_SIZE, TAG, "image is %ux%u, need %dx%d",
                        (unsigned)info.width, (unsigned)info.height, UI_LCD_H_RES, UI_LCD_V_RES);

    const uint32_t out_w = round_up_block(info.width);
    const uint32_t out_h = round_up_block(info.height);
    const jpeg_decode_memory_alloc_cfg_t in_mem = {.buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER};
    const jpeg_decode_memory_alloc_cfg_t out_mem = {.buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER};
    size_t in_cap = 0;
    size_t out_cap = 0;
    /* 輸入也要複製到 DMA 可用的記憶體：Flash 上的資料不能直接給硬體解碼 */
    uint8_t *in = jpeg_alloc_decoder_mem(jpg_size, &in_mem, &in_cap);
    uint8_t *out = jpeg_alloc_decoder_mem(out_w * out_h * RGB565_BYTES_PER_PX, &out_mem, &out_cap);
    jpeg_decoder_handle_t engine = NULL;
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(in != NULL && out != NULL, ESP_ERR_NO_MEM, fail, TAG, "no memory");
    memcpy(in, jpg, jpg_size);

    const jpeg_decode_engine_cfg_t engine_cfg = {.timeout_ms = JPEG_TIMEOUT_MS};
    ESP_GOTO_ON_ERROR(jpeg_new_decoder_engine(&engine_cfg, &engine), fail, TAG, "jpeg engine");
    const jpeg_decode_cfg_t cfg = {
        .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,  /* 低位元組在前，同 LVGL 的 RGB565 */
        .conv_std = JPEG_YUV_RGB_CONV_STD_BT601,
    };
    uint32_t out_len = 0;
    ESP_GOTO_ON_ERROR(jpeg_decoder_process(engine, &cfg, in, jpg_size, out, out_cap, &out_len),
                      fail, TAG, "jpeg decode");
    jpeg_del_decoder_engine(engine);
    free(in);
    *pixels = out;
    *stride = out_w * RGB565_BYTES_PER_PX;
    return ESP_OK;

fail:
    if (engine != NULL) {
        jpeg_del_decoder_engine(engine);
    }
    free(in);
    free(out);
    return ret;
}

esp_err_t ui_image_load_standby(const lv_image_dsc_t **out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t *pixels = NULL;
    uint32_t stride = 0;
    const uint8_t *jpg = NULL;
    uint32_t jpg_size = 0;
    const char *source = "venue image";
    /* 先用現場匯入的圖；沒有、或解不開（尺寸不對、progressive JPEG）就用編進韌體的預設圖 */
    if (standby_store_get(&jpg, &jpg_size) != ESP_OK ||
        decode(jpg, jpg_size, &pixels, &stride) != ESP_OK) {
        source = "built-in image";
        jpg = standby_jpg_start;
        jpg_size = (uint32_t)(standby_jpg_end - standby_jpg_start);
        ESP_RETURN_ON_ERROR(decode(jpg, jpg_size, &pixels, &stride), TAG, "standby image");
    }

    s_standby = (lv_image_dsc_t){
        .header = {
            .magic = LV_IMAGE_HEADER_MAGIC,
            .cf = LV_COLOR_FORMAT_RGB565,
            .w = UI_LCD_H_RES,
            .h = UI_LCD_V_RES,
            .stride = stride,
        },
        .data_size = stride * UI_LCD_V_RES,
        .data = pixels,
    };
    *out = &s_standby;
    ESP_LOGI(TAG, "standby: %s, %u bytes jpeg", source, (unsigned)jpg_size);
    return ESP_OK;
}
