#include "camera.h"
#include "code128.h"

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
#define CAM_STATS_PERIOD_US (30 * 1000000LL)
#define CAM_STATUS_HOLD_MS  1500         /* 畫面上的掃描狀態至少維持這麼久 */
#define CAM_FAIL_LOG_PERIOD_MS 2000      /* 讀不出來的原因最多多久印一次 */
#define CAM_SENSOR_MIRRORED true         /* 感光元件輸出左右相反（2026-11-23 實拍 QR Code 的定位點確認） */
#define CAM_PAYLOAD_LOG_MAX 24           /* log 最多印幾個 bytes 的 QR 內容 */
#define BARCODE_ROWS        12           /* 每張影像橫切幾列找一維條碼 */
#define BARCODE_TOP_PCT     15           /* 從影像高度的幾 % 開始切 */
#define BARCODE_BOTTOM_PCT  85
#define BARCODE_CONFIRM_MS  2000         /* 一維條碼的檢查碼比較弱：這段時間內讀到兩次相同內容才算數 */
#define CAM_MAX_WIDTH       1920
#define DUMP_STEP           2            /* 除錯影像再縮一半，傳輸比較快 */

static camera_code_cb_t s_cb;
static void *s_cb_ctx;
static volatile bool s_dump_next;
static camera_observer_t s_observer;

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

/* 取出影像的第 y 列（綠色通道），需要時左右對調 */
static void gray_row(const uint8_t *rgb565, int w, int y, uint8_t *row)
{
    const uint8_t *p = rgb565 + (size_t)y * (size_t)w * 2;
    for (int x = 0; x < w; x++, p += 2) {
        const unsigned g = ((unsigned)(p[0] | (p[1] << 8)) >> 5) & 0x3F;
        row[CAM_SENSOR_MIRRORED ? w - 1 - x : x] = (uint8_t)(g << 2);
    }
}

/*
 * 在原始解析度的影像上橫切幾列找 Code 128 條碼。
 * 連續兩次讀到相同內容才回傳 true 並把領球碼寫到 code。
 * *partial：有任何一列只拍到半截條碼（手機太近）。
 */
static bool scan_barcode(const uint8_t *rgb565, int w, int h, uint8_t *row, char *code,
                         size_t code_size, bool *partial)
{
    *partial = false;
    static char s_pending[CODE128_TEXT_MAX];
    static uint32_t s_pending_ms;
    char text[CODE128_TEXT_MAX];
    for (int i = 0; i < BARCODE_ROWS; i++) {
        const int pct = BARCODE_TOP_PCT + (BARCODE_BOTTOM_PCT - BARCODE_TOP_PCT) * i / (BARCODE_ROWS - 1);
        gray_row(rgb565, w, h * pct / 100, row);
        const code128_result_t result = code128_scan_row(row, w, text, sizeof text);
        if (result != CODE128_OK) {
            *partial = *partial || result == CODE128_PARTIAL;
            continue;
        }
        const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        const bool confirmed = strcmp(text, s_pending) == 0 &&
                               (uint32_t)(now_ms - s_pending_ms) <= BARCODE_CONFIRM_MS;
        strlcpy(s_pending, text, sizeof s_pending);
        s_pending_ms = now_ms;
        if (!confirmed) {
            return false;  /* 第一次看到：等下一張影像確認 */
        }
        s_pending[0] = '\0';
        ESP_LOGI(TAG, "barcode raw: %s", text);
        return qr_code_from_payload((const uint8_t *)text, strlen(text), code, code_size);
    }
    return false;
}

/* 讀不出來的原因：限制頻率，否則碼停在鏡頭前時每張影像都印一行 */
static void log_decode_failure(const char *reason)
{
    static uint32_t s_last_ms;
    const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    if ((uint32_t)(now_ms - s_last_ms) >= CAM_FAIL_LOG_PERIOD_MS) {
        s_last_ms = now_ms;
        ESP_LOGI(TAG, "qr found but not decoded: %s", reason);
    }
}

/* 印出 QR 的原始內容（十六進位），限制頻率。用來確認 App 產生的 QR Code 到底寫了什麼。 */
static void log_payload(const struct quirc_data *data, bool flipped)
{
    static uint32_t s_last_ms;
    const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    if ((uint32_t)(now_ms - s_last_ms) < CAM_FAIL_LOG_PERIOD_MS) {
        return;
    }
    s_last_ms = now_ms;
    char hex[CAM_PAYLOAD_LOG_MAX * 2 + 1] = "";
    const int n = data->payload_len < CAM_PAYLOAD_LOG_MAX ? data->payload_len : CAM_PAYLOAD_LOG_MAX;
    for (int i = 0; i < n; i++) {
        snprintf(hex + i * 2, 3, "%02x", data->payload[i]);
    }
    ESP_LOGI(TAG, "qr raw: version=%d ecc=%d type=%d len=%d flipped=%d hex=%s", data->version,
             data->ecc_level, data->data_type, data->payload_len, (int)flipped, hex);
}

/*
 * 辨識 quirc 找到的每一張碼，把合格的領球碼送出。
 * 回傳這張影像的結果（給畫面用）；*decoded 是成功解出的張數。
 */
static qr_scan_state_t decode_all(struct quirc *q, struct quirc_code *code,
                                  struct quirc_data *data, int *decoded_out)
{
    int decoded = 0;
    const int count = quirc_count(q);
    qr_scan_state_t seen = count > 0 ? QR_SCAN_FOUND : QR_SCAN_IDLE;
    for (int i = 0; i < count; i++) {
        quirc_extract(q, i, code);
        quirc_decode_error_t err = quirc_decode(code, data);
        bool flipped = false;
        if (err == QUIRC_ERROR_DATA_ECC) {
            quirc_flip(code);  /* 鏡像的碼：翻過來再試一次 */
            err = quirc_decode(code, data);
            flipped = true;
        }
        if (err != QUIRC_SUCCESS) {
            log_decode_failure(quirc_strerror(err));
            continue;
        }
        decoded++;
        log_payload(data, flipped);
        char text[QR_CODE_MAX];
        if (!qr_code_from_payload(data->payload, (size_t)data->payload_len, text, sizeof text)) {
            ESP_LOGW(TAG, "qr content is not a code (%d bytes)", data->payload_len);
            continue;
        }
        /*
         * 每次讀到都交給主流程，不判斷是不是剛讀過（Jeff 2026-11-23）。
         * 主流程只在待機與輸入畫面接受；處理中的那幾秒會略過。
         */
        ESP_LOGI(TAG, "qr code: %s", text);
        seen = QR_SCAN_READ;
        if (s_cb != NULL) {
            s_cb(text, s_cb_ctx);
        }
    }
    *decoded_out = decoded;
    return seen;
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
    uint8_t *row = w > 0 && w <= CAM_MAX_WIDTH ? heap_caps_malloc((size_t)w, MALLOC_CAP_SPIRAM) : NULL;
    if (fd < 0 || q == NULL || code == NULL || data == NULL || row == NULL ||
        quirc_resize(q, gw, gh) != 0) {
        ESP_LOGE(TAG, "camera task cannot start; QR scanning is off");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "streaming %dx%d, scanning QR at %dx%d", w, h, gw, gh);

    qr_status_t status = {0};
    qr_scan_state_t reported = QR_SCAN_IDLE;
    int frames = 0;
    int decoded = 0;
    int found_frames = 0;
    int partial_frames = 0;
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
        /*
         * 兩種取法輪流：整張縮一半（近的、大的碼），中間不縮小（遠的、小的碼）。
         * 兩種的像素數相同，辨識時間不變。預覽只用整張的那一種。
         */
        const bool whole = frames % 2 == 0;
        if (whole) {
            qr_gray_half_from_rgb565(bufs[b.index], w, h, gray, CAM_SENSOR_MIRRORED);
        } else {
            qr_gray_crop_from_rgb565(bufs[b.index], w, h, gw, gh, gray, CAM_SENSOR_MIRRORED);
        }
        char barcode[QR_CODE_MAX];
        bool barcode_partial = false;
        const bool barcode_read =
            scan_barcode(bufs[b.index], w, h, row, barcode, sizeof barcode, &barcode_partial);
        if (ioctl(fd, VIDIOC_QBUF, &b) != 0) {
            ESP_LOGW(TAG, "QBUF failed (errno %d)", errno);
        }
        if (s_dump_next) {
            s_dump_next = false;
            dump_gray(gray, gw, gh);  /* quirc_end 會改寫緩衝區，所以先印 */
        }
        if (s_observer.on_preview != NULL && whole) {
            s_observer.on_preview(gray, gw, gh, s_observer.ctx);
        }
        quirc_end(q);
        int read = 0;
        qr_scan_state_t seen = decode_all(q, code, data, &read);
        if (barcode_partial && seen < QR_SCAN_TOO_CLOSE) {
            seen = QR_SCAN_TOO_CLOSE;
            partial_frames++;
        }
        if (barcode_read) {
            ESP_LOGI(TAG, "barcode code: %s", barcode);
            seen = QR_SCAN_READ;
            read++;
            if (s_cb != NULL) {
                s_cb(barcode, s_cb_ctx);
            }
        }
        decoded += read;
        found_frames += seen != QR_SCAN_IDLE ? 1 : 0;
        work_us += esp_timer_get_time() - t0;
        frames++;

        const qr_scan_state_t shown = qr_status_update(
            &status, seen, (uint32_t)(esp_timer_get_time() / 1000), CAM_STATUS_HOLD_MS);
        if (shown != reported) {
            reported = shown;
            if (s_observer.on_status != NULL) {
                s_observer.on_status((int)shown, s_observer.ctx);
            }
        }

        const int64_t now = esp_timer_get_time();
        if (now - stats_t0 >= CAM_STATS_PERIOD_US) {
            ESP_LOGI(TAG, "%d frames, %d with code, %d cut-off barcode, %d decoded, %lld ms per frame",
                     frames, found_frames, partial_frames, decoded,
                     (long long)(work_us / 1000 / (frames > 0 ? frames : 1)));
            partial_frames = 0;
            frames = 0;
            decoded = 0;
            found_frames = 0;
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

void camera_set_observer(const camera_observer_t *observer)
{
    s_observer = observer != NULL ? *observer : (camera_observer_t){0};
}

void camera_debug_dump_next(void)
{
    s_dump_next = true;
}
