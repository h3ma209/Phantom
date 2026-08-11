#include "hid_kbd.h"

#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_hidd.h"
#include "esp_hid_gap.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if CONFIG_BT_NIMBLE_ENABLED
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#endif

static const char *TAG = "hid_kbd";

#define USB_HID_MODIFIER_LEFT_SHIFT 0x02
#define USB_HID_SPACE               0x2C
#define USB_HID_NEWLINE             0x28
#define USB_HID_DOT                 0x37
#define USB_HID_FSLASH              0x38
#define USB_HID_COMMA               0x36

static const unsigned char s_kbd_map[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01, 0x05, 0x07, 0x19, 0xE0,
    0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x03, 0x95, 0x05, 0x75, 0x01, 0x05, 0x08,
    0x19, 0x01, 0x29, 0x05, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x03,
    0x95, 0x05, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05, 0x07, 0x19, 0x00,
    0x29, 0x65, 0x81, 0x00, 0xC0,
};

static esp_hid_raw_report_map_t s_maps[] = {
    {.data = s_kbd_map, .len = sizeof(s_kbd_map)},
};

static esp_hid_device_config_t s_cfg = {
    .vendor_id = 0x16C0,
    .product_id = 0x05DF,
    .version = 0x0100,
    .device_name = "DEDSEC KBD",
    .manufacturer_name = "DEDSEC",
    .serial_number = "0001",
    .report_maps = s_maps,
    .report_maps_len = 1,
};

static esp_hidd_dev_t *s_dev;
static volatile bool s_connected;
static bool s_started;

#if CONFIG_BT_NIMBLE_ENABLED
void ble_store_config_init(void);

/* Called from esp_hid_gap.c on NimBLE GAP connect. */
void ble_hid_task_start_up(void)
{
}

static void hid_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}
#endif

static void hid_cb(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    (void)base;
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;

    switch (event) {
    case ESP_HIDD_START_EVENT:
        ESP_LOGI(TAG, "advertising");
        esp_hid_ble_gap_adv_start();
        break;
    case ESP_HIDD_CONNECT_EVENT:
        ESP_LOGI(TAG, "connected");
        s_connected = true;
        break;
    case ESP_HIDD_DISCONNECT_EVENT:
        ESP_LOGI(TAG, "disconnected");
        s_connected = false;
        esp_hid_ble_gap_adv_start();
        break;
    case ESP_HIDD_STOP_EVENT:
        s_connected = false;
        break;
    default:
        (void)param;
        break;
    }
}

static void char_to_code(uint8_t *buf, char ch)
{
    memset(buf, 0, 8);
    if (ch >= 'a' && ch <= 'z') {
        buf[2] = (uint8_t)(4 + (ch - 'a'));
    } else if (ch >= 'A' && ch <= 'Z') {
        buf[0] = USB_HID_MODIFIER_LEFT_SHIFT;
        buf[2] = (uint8_t)(4 + (ch - 'A'));
    } else if (ch >= '1' && ch <= '9') {
        buf[2] = (uint8_t)(30 + (ch - '1'));
    } else if (ch == '0') {
        buf[2] = 39;
    } else if (ch == ' ') {
        buf[2] = USB_HID_SPACE;
    } else if (ch == '\n') {
        buf[2] = USB_HID_NEWLINE;
    } else if (ch == '.') {
        buf[2] = USB_HID_DOT;
    } else if (ch == ',') {
        buf[2] = USB_HID_COMMA;
    } else if (ch == '/') {
        buf[2] = USB_HID_FSLASH;
    } else if (ch == '-') {
        buf[2] = 0x2D;
    } else if (ch == '\'') {
        buf[2] = 0x34;
    }
}

esp_err_t hid_kbd_start(void)
{
    if (s_started) {
        return ESP_OK;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "nvs");

    ESP_RETURN_ON_ERROR(esp_hid_gap_init(HIDD_BLE_MODE), TAG, "gap init");
    ESP_RETURN_ON_ERROR(
        esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_KEYBOARD, s_cfg.device_name), TAG, "adv init");
    ESP_RETURN_ON_ERROR(
        esp_hidd_dev_init(&s_cfg, ESP_HID_TRANSPORT_BLE, hid_cb, &s_dev), TAG, "hidd init");

#if CONFIG_BT_NIMBLE_ENABLED
    ble_store_config_init();
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ret = esp_nimble_enable(hid_host_task);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble enable %d", ret);
        return ret;
    }
#endif

    s_started = true;
    ESP_LOGI(TAG, "HID keyboard advertising as '%s'", s_cfg.device_name);
    return ESP_OK;
}

bool hid_kbd_is_connected(void)
{
    return s_connected;
}

void hid_kbd_type(const char *text)
{
    if (!s_connected || !s_dev || !text) {
        return;
    }
    uint8_t buf[8];
    for (const char *p = text; *p; p++) {
        char_to_code(buf, *p);
        if (buf[2] == 0 && *p != 0) {
            continue;
        }
        esp_hidd_dev_input_set(s_dev, 0, 1, buf, 8);
        vTaskDelay(pdMS_TO_TICKS(40));
        memset(buf, 0, sizeof(buf));
        esp_hidd_dev_input_set(s_dev, 0, 1, buf, 8);
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}
