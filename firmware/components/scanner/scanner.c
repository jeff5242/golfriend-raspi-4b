#include "scanner.h"

#include <stdatomic.h>
#include <stdbool.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "hid_keymap.h"
#include "scanner_devs.h"
#include "usb/hid_host.h"
#include "usb/usb_host.h"

static const char *TAG = "scanner";

#define HID_REPORT_MAX          64
#define USB_TASK_STACK          4096
#define USB_TASK_PRIO           5

static scanner_line_cb_t s_on_line;
static scanner_key_cb_t s_on_key;
static void *s_ctx;
/*
 * 每台 HID 裝置各自的狀態。usb_host_hid 在同一個背景工作呼叫所有裝置的
 * hid_interface_cb，表只在那個工作中存取，不必上鎖。
 */
static scanner_devs_t s_devs;
static scanner_dev_t s_serial_dev;   /* 序列埠讀卡機（預留），由它自己的驅動工作存取 */
static atomic_uint s_reset_gen;      /* 每回待機一次加 1，各裝置收到下一個字元前比對 */
static QueueHandle_t s_dev_queue;

typedef struct {
    hid_host_device_handle_t handle;
    hid_host_driver_event_t event;
} hid_dev_event_t;

/* ---- 字元 → 一行 ------------------------------------------------------ */

void scanner_reset_line(void)
{
    atomic_fetch_add(&s_reset_gen, 1u);
}

static void feed_char(scanner_dev_t *dev, char c)
{
    const int64_t now = esp_timer_get_time();
    const bool printable = c >= 0x20 && c < 0x7F && c != '%';
    if (printable && now >= dev->line.guard_until_us && s_on_key != NULL) {
        s_on_key(c, s_ctx);
    }
    char out[SCANNER_LINE_MAX];
    if (scanner_dev_feed(dev, c, now, atomic_load(&s_reset_gen), out, sizeof out)) {
        ESP_LOGI(TAG, "line: %s", out);
        if (s_on_line != NULL) {
            s_on_line(out, s_ctx);
        }
    }
}

void scanner_handle_keyboard_report(const void *device, const uint8_t *report, size_t len)
{
    scanner_dev_t *dev = scanner_devs_get(&s_devs, device, atomic_load(&s_reset_gen));
    if (dev == NULL) {
        ESP_LOGW(TAG, "too many input devices (max %d), report dropped", SCANNER_MAX_DEVICES);
        return;
    }
    char chars[HID_KBD_KEY_SLOTS];
    const size_t n = scanner_dev_report_chars(dev, report, len, chars);
    for (size_t i = 0; i < n; i++) {
        feed_char(dev, chars[i]);
    }
}

void scanner_handle_serial_bytes(const uint8_t *data, size_t len)
{
    for (size_t i = 0; data != NULL && i < len; i++) {
        feed_char(&s_serial_dev, (char)data[i]);
    }
}

/* ---- USB Host / HID 驅動（依 ESP-IDF examples/peripherals/usb/host/hid） ---- */

static void hid_interface_cb(hid_host_device_handle_t handle,
                             const hid_host_interface_event_t event, void *arg)
{
    (void)arg;
    uint8_t data[HID_REPORT_MAX];
    size_t data_len = 0;
    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        if (hid_host_device_get_raw_input_report_data(handle, data, sizeof data, &data_len) ==
            ESP_OK) {
            scanner_handle_keyboard_report(handle, data, data_len);
        }
        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HID device disconnected");
        scanner_devs_remove(&s_devs, handle);  /* 只清這台，其他裝置沒打完的字保留 */
        hid_host_device_close(handle);
        break;
    default:
        break;
    }
}

/* 驅動回呼不得做阻塞動作，只把事件丟到佇列，由 hid_dev_task 處理 */
static void hid_driver_cb(hid_host_device_handle_t handle, const hid_host_driver_event_t event,
                          void *arg)
{
    (void)arg;
    const hid_dev_event_t ev = {.handle = handle, .event = event};
    xQueueSend(s_dev_queue, &ev, 0);
}

static void open_keyboard(hid_host_device_handle_t handle)
{
    const hid_host_device_config_t dev_cfg = {.callback = hid_interface_cb, .callback_arg = NULL};
    if (hid_host_device_open(handle, &dev_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "hid_host_device_open failed");
        return;
    }
    hid_host_dev_params_t params;
    hid_host_device_get_params(handle, &params);
    if (params.proto != HID_PROTOCOL_KEYBOARD) {
        /* 廠商自訂 HID 的讀卡機會走到這裡，必須照廠商文件另外解析 */
        ESP_LOGW(TAG, "HID device is not a boot keyboard (proto=%d), ignored", params.proto);
        hid_host_device_close(handle);
        return;
    }
    if (params.sub_class == HID_SUBCLASS_BOOT_INTERFACE) {
        hid_class_request_set_protocol(handle, HID_REPORT_PROTOCOL_BOOT);
        hid_class_request_set_idle(handle, 0, 0);
    }
    const esp_err_t err = hid_host_device_start(handle);
    if (err != ESP_OK) {
        /* 沒啟動就收不到任何按鍵：關掉讓 log 看得出來，不要假裝已連上 */
        ESP_LOGE(TAG, "hid_host_device_start: %s", esp_err_to_name(err));
        hid_host_device_close(handle);
        return;
    }
    ESP_LOGI(TAG, "HID keyboard connected");
}

static void hid_dev_task(void *arg)
{
    (void)arg;
    hid_dev_event_t ev;
    for (;;) {
        if (xQueueReceive(s_dev_queue, &ev, portMAX_DELAY) == pdTRUE &&
            ev.event == HID_HOST_DRIVER_EVENT_CONNECTED) {
            open_keyboard(ev.handle);
        }
    }
}

static void usb_lib_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t flags = 0;
        usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            usb_host_device_free_all();
        }
    }
}

esp_err_t scanner_start(scanner_line_cb_t on_line, scanner_key_cb_t on_key, void *ctx)
{
    s_on_line = on_line;
    s_on_key = on_key;
    s_ctx = ctx;
    scanner_devs_init(&s_devs);
    scanner_line_init(&s_serial_dev.line);

    s_dev_queue = xQueueCreate(8, sizeof(hid_dev_event_t));
    if (s_dev_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const usb_host_config_t host_cfg = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    esp_err_t err = usb_host_install(&host_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "usb_host_install: %s", esp_err_to_name(err));
        return err;
    }
    xTaskCreate(usb_lib_task, "usb_lib", USB_TASK_STACK, NULL, USB_TASK_PRIO, NULL);

    const hid_host_driver_config_t hid_cfg = {
        .create_background_task = true,
        .task_priority = USB_TASK_PRIO,
        .stack_size = USB_TASK_STACK,
        .core_id = 0,
        .callback = hid_driver_cb,
        .callback_arg = NULL,
    };
    err = hid_host_install(&hid_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "hid_host_install: %s", esp_err_to_name(err));
        return err;
    }
    xTaskCreate(hid_dev_task, "hid_dev", USB_TASK_STACK, NULL, USB_TASK_PRIO, NULL);
    ESP_LOGI(TAG, "USB host ready, waiting for scanner / keyboard / card reader");
    return ESP_OK;
}
