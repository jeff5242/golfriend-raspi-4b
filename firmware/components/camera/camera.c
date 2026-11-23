#include "camera.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "linux/videodev2.h"
#include "qr_code.h"
#include "quirc.h"

static const char *TAG = "camera";

#define CAM_I2C_PORT        0      /* 和觸控（GT911）共用 */
#define CAM_SCCB_FREQ_HZ    100000
#define CAM_BUF_COUNT       2
#define CAM_TASK_STACK      (32 * 1024)  /* quirc 解碼會用到數 KB 的堆疊 */
#define CAM_TASK_PRIORITY   3            /* 低於畫面與主流程 */
#define CAM_FRAME_GAP_MS    80           /* 兩張影像之間讓出 CPU */
#define CAM_SAME_CODE_HOLD_MS 5000       /* 同一張碼離開鏡頭多久後可以再送一次 */
#define CAM_STATS_PERIOD_US (30 * 1000000LL)
#define DUMP_STEP           2            /* 除錯影像再縮一半，傳輸比較快 */

static camera_code_cb_t s_cb;
static void *s_cb_ctx;
static volatile bool s_dump_next;

static esp_err_t video_init(void)
{
    i2c_master_bus_handle_t bus = NULL;
    ESP_RETURN_ON_ERROR(i2c_master_get_bus_handle(CAM_I2C_PORT, &bus), TAG, "i2c bus not ready");
    const esp_video_init_csi_config_t csi = {
        .sccb_config = {.init_sccb = false, .i2c_handle = bus, .freq = CAM_SCCB_FREQ_HZ},
        .reset_pin = -1,  /* 板上沒有接到 P4 的重置／斷電腳位 */
        .pwdn_pin = -1,
        .dont_init_ldo = true,  /* MIPI D-PHY 的 LDO 已經由螢幕（DSI）開啟 */
    };
    const esp_video_init_config_t cfg = {.csi = &csi};
    return esp_video_init(&cfg);
}

/* 開啟裝置、設成 RGB565、配置緩衝區並開始串流。成功回傳 fd，失敗回傳 -1。 */
static int stream_open(void *bufs[CAM_BUF_COUNT], int *w, int *h)
{
    const int fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "open %s failed (errno %d)", ESP_VIDEO_MIPI_CSI_DEVICE_NAME, errno);
        return -1;
    }
    struct v4l2_format fmt = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE};
    if (ioctl(fd, VIDIOC_G_FMT, &fmt) != 0) {
        ESP_LOGE(TAG, "G_FMT failed (errno %d)", errno);
        close(fd);
        return -1;
    }
    *w = (int)fmt.fmt.pix.width;
    *h = (int)fmt.fmt.pix.height;
    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565) {
        struct v4l2_format set = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE};
        set.fmt.pix.width = fmt.fmt.pix.width;
        set.fmt.pix.height = fmt.fmt.pix.height;
        set.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
        if (ioctl(fd, VIDIOC_S_FMT, &set) != 0) {
            ESP_LOGE(TAG, "RGB565 not supported (errno %d)", errno);
            close(fd);
            return -1;
        }
    }
    struct v4l2_requestbuffers req = {
        .count = CAM_BUF_COUNT, .type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .memory = V4L2_MEMORY_MMAP};
    if (ioctl(fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "REQBUFS failed (errno %d)", errno);
        close(fd);
        return -1;
    }
    for (int i = 0; i < CAM_BUF_COUNT; i++) {
        struct v4l2_buffer b = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .memory = V4L2_MEMORY_MMAP, .index = i};
        if (ioctl(fd, VIDIOC_QUERYBUF, &b) != 0) {
            ESP_LOGE(TAG, "QUERYBUF failed (errno %d)", errno);
            close(fd);
            return -1;
        }
        bufs[i] = mmap(NULL, b.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, b.m.offset);
        if (bufs[i] == MAP_FAILED || ioctl(fd, VIDIOC_QBUF, &b) != 0) {
            ESP_LOGE(TAG, "buffer %d setup failed (errno %d)", i, errno);
            close(fd);
            return -1;
        }
    }
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "STREAMON failed (errno %d)", errno);
        close(fd);
        return -1;
    }
    return fd;
}

static void dump_gray(const uint8_t *gray, int w, int h)
{
    printf("CAMDUMP %d %d\n", w / DUMP_STEP, h / DUMP_STEP);
    for (int y = 0; y + DUMP_STEP <= h; y += DUMP_STEP) {
        for (int x = 0; x + DUMP_STEP <= w; x += DUMP_STEP) {
            printf("%02x", gray[y * w + x]);
        }
        printf("\n");
    }
    printf("CAMDUMP-END\n");
}

/* 辨識 quirc 找到的每一張碼，把合格的領球碼送出。回傳成功解出的張數。 */
static int decode_all(struct quirc *q, struct quirc_code *code, struct quirc_data *data,
                      qr_gate_t *gate)
{
    int decoded = 0;
    const int count = quirc_count(q);
    for (int i = 0; i < count; i++) {
        quirc_extract(q, i, code);
        quirc_decode_error_t err = quirc_decode(code, data);
        if (err == QUIRC_ERROR_DATA_ECC) {
            quirc_flip(code);  /* 鏡像的碼：翻過來再試一次 */
            err = quirc_decode(code, data);
        }
        if (err != QUIRC_SUCCESS) {
            ESP_LOGD(TAG, "qr found but not decoded: %s", quirc_strerror(err));
            continue;
        }
        decoded++;
        char text[QR_CODE_MAX];
        if (!qr_code_from_payload(data->payload, (size_t)data->payload_len, text, sizeof text)) {
            ESP_LOGW(TAG, "qr content is not a code (%d bytes)", data->payload_len);
            continue;
        }
        const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        if (qr_gate_accept(gate, text, now_ms, CAM_SAME_CODE_HOLD_MS)) {
            ESP_LOGI(TAG, "qr code: %s", text);
            if (s_cb != NULL) {
                s_cb(text, s_cb_ctx);
            }
        }
    }
    return decoded;
}

static void camera_task(void *arg)
{
    (void)arg;
    void *bufs[CAM_BUF_COUNT];
    int w = 0;
    int h = 0;
    const int fd = stream_open(bufs, &w, &h);
    struct quirc *q = quirc_new();
    struct quirc_code *code = heap_caps_malloc(sizeof *code, MALLOC_CAP_SPIRAM);
    struct quirc_data *data = heap_caps_malloc(sizeof *data, MALLOC_CAP_SPIRAM);
    const int gw = w / 2;
    const int gh = h / 2;
    if (fd < 0 || q == NULL || code == NULL || data == NULL || quirc_resize(q, gw, gh) != 0) {
        ESP_LOGE(TAG, "camera task cannot start; QR scanning is off");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "streaming %dx%d, scanning QR at %dx%d", w, h, gw, gh);

    qr_gate_t gate = {0};
    int frames = 0;
    int decoded = 0;
    int64_t work_us = 0;
    int64_t stats_t0 = esp_timer_get_time();
    for (;;) {
        struct v4l2_buffer b = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .memory = V4L2_MEMORY_MMAP};
        if (ioctl(fd, VIDIOC_DQBUF, &b) != 0) {
            ESP_LOGW(TAG, "DQBUF failed (errno %d)", errno);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        const int64_t t0 = esp_timer_get_time();
        uint8_t *gray = quirc_begin(q, NULL, NULL);
        qr_gray_half_from_rgb565(bufs[b.index], w, h, gray);
        if (ioctl(fd, VIDIOC_QBUF, &b) != 0) {
            ESP_LOGW(TAG, "QBUF failed (errno %d)", errno);
        }
        if (s_dump_next) {
            s_dump_next = false;
            dump_gray(gray, gw, gh);  /* quirc_end 會改寫緩衝區，所以先印 */
        }
        quirc_end(q);
        decoded += decode_all(q, code, data, &gate);
        work_us += esp_timer_get_time() - t0;
        frames++;

        const int64_t now = esp_timer_get_time();
        if (now - stats_t0 >= CAM_STATS_PERIOD_US) {
            ESP_LOGI(TAG, "%d frames, %d decoded, %lld ms per frame", frames, decoded,
                     (long long)(work_us / 1000 / (frames > 0 ? frames : 1)));
            frames = 0;
            decoded = 0;
            work_us = 0;
            stats_t0 = now;
        }
        vTaskDelay(pdMS_TO_TICKS(CAM_FRAME_GAP_MS));
    }
}

esp_err_t camera_start(camera_code_cb_t cb, void *ctx)
{
    s_cb = cb;
    s_cb_ctx = ctx;
    ESP_RETURN_ON_ERROR(video_init(), TAG, "video init");
    if (xTaskCreate(camera_task, "camera", CAM_TASK_STACK, NULL, CAM_TASK_PRIORITY, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void camera_debug_dump_next(void)
{
    s_dump_next = true;
}
