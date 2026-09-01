/**
 * BLE scan + advertising clone for Evil Bluetooth UI.
 */
#include "ble_clone.h"

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"

#include "ble_hid_gap.h"
#include "ble_hid_keyboard.h"

#if CONFIG_BT_NIMBLE_ENABLED
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_hs_adv.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

void ble_store_config_init(void);
#endif

static const char *TAG = "ble_clone";

static ble_clone_dev_t s_devs[BLE_CLONE_MAX_DEVS];
static int s_count;
static volatile bool s_scanning;
static volatile bool s_cloning;
static char s_clone_name[BLE_CLONE_NAME_LEN];
static uint32_t s_list_gen;
static SemaphoreHandle_t s_lock;
static int s_scan_rc; /* last disc start rc */

#if CONFIG_BT_NIMBLE_ENABLED
static void ble_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void lock(void)
{
    if (s_lock) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
    }
}

static void unlock(void)
{
    if (s_lock) {
        xSemaphoreGive(s_lock);
    }
}

static void sanitize_name(char *s)
{
    for (; *s; s++) {
        if (*s < 32 || *s > 126) {
            *s = '?';
        }
    }
}

/** Prefer Complete Local Name (0x09), else Shortened (0x08). Never hex MAC. */
static void parse_name(const uint8_t *data, uint8_t len, char *out, size_t out_sz, bool *named)
{
    out[0] = 0;
    *named = false;
    if (!data || len == 0 || out_sz < 2) {
        return;
    }

    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));
    if (ble_hs_adv_parse_fields(&fields, data, len) == 0 &&
        fields.name != NULL && fields.name_len > 0) {
        size_t n = fields.name_len;
        if (n > out_sz - 1) {
            n = out_sz - 1;
        }
        memcpy(out, fields.name, n);
        out[n] = 0;
        sanitize_name(out);
        *named = (out[0] != 0);
        return;
    }

    /* Manual walk — prefer type 0x09 over 0x08 */
    const char *found = NULL;
    int found_n = 0;
    bool complete = false;
    for (int i = 0; i < len;) {
        uint8_t flen = data[i];
        if (flen == 0 || i + 1 + flen > len) {
            break;
        }
        uint8_t type = data[i + 1];
        if ((type == 0x09 || type == 0x08) && flen >= 2) {
            int n = flen - 1;
            if (type == 0x09 || !complete) {
                found = (const char *)&data[i + 2];
                found_n = n;
                complete = (type == 0x09);
            }
            if (type == 0x09) {
                break;
            }
        }
        i += 1 + flen;
    }
    if (found && found_n > 0) {
        if (found_n > (int)out_sz - 1) {
            found_n = (int)out_sz - 1;
        }
        memcpy(out, found, found_n);
        out[found_n] = 0;
        sanitize_name(out);
        *named = (out[0] != 0);
    }
}

static int find_dev(const uint8_t *addr)
{
    for (int i = 0; i < s_count; i++) {
        if (memcmp(s_devs[i].addr, addr, 6) == 0) {
            return i;
        }
    }
    return -1;
}

static void upsert_dev(const struct ble_gap_disc_desc *desc)
{
    if (!desc || desc->length_data == 0 || !desc->data) {
        return;
    }

    char name[BLE_CLONE_NAME_LEN];
    bool named = false;
    parse_name(desc->data, desc->length_data, name, sizeof(name), &named);

    lock();
    int idx = find_dev(desc->addr.val);
    if (idx < 0) {
        if (s_count >= BLE_CLONE_MAX_DEVS) {
            int weak = 0;
            for (int i = 1; i < s_count; i++) {
                if (s_devs[i].rssi < s_devs[weak].rssi) {
                    weak = i;
                }
            }
            /* Prefer keeping named devices over unnamed when full */
            if (!named) {
                for (int i = 0; i < s_count; i++) {
                    if (!s_devs[i].named && s_devs[i].rssi <= s_devs[weak].rssi) {
                        weak = i;
                    }
                }
            }
            if (desc->rssi <= s_devs[weak].rssi && !(named && !s_devs[weak].named)) {
                unlock();
                return;
            }
            idx = weak;
        } else {
            idx = s_count++;
        }
        memset(&s_devs[idx], 0, sizeof(s_devs[idx]));
        memcpy(s_devs[idx].addr, desc->addr.val, 6);
        s_devs[idx].addr_type = desc->addr.type;
        strncpy(s_devs[idx].name, "Unknown", BLE_CLONE_NAME_LEN - 1);
    }

    s_devs[idx].rssi = desc->rssi;
    if (named) {
        strncpy(s_devs[idx].name, name, BLE_CLONE_NAME_LEN - 1);
        s_devs[idx].name[BLE_CLONE_NAME_LEN - 1] = 0;
        s_devs[idx].named = true;
    }

    uint8_t copy_len = desc->length_data;
    if (copy_len > BLE_CLONE_ADV_MAX) {
        copy_len = BLE_CLONE_ADV_MAX;
    }
    /* Keep AD that carries the name / longest payload */
    if (named || copy_len >= s_devs[idx].adv_len) {
        memcpy(s_devs[idx].adv, desc->data, copy_len);
        s_devs[idx].adv_len = copy_len;
    }

    s_list_gen++;
    unlock();
}

static int on_gap(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        upsert_dev(&event->disc);
        return 0;
    case BLE_GAP_EVENT_DISC_COMPLETE:
        ESP_LOGI(TAG, "scan complete reason=%d count=%d", event->disc_complete.reason, s_count);
        s_scanning = false;
        return 0;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        return 0;
    default:
        return 0;
    }
}

esp_err_t ble_clone_ensure_stack(void)
{
    if (!s_lock) {
        s_lock = xSemaphoreCreateMutex();
    }

    if (ble_hid_keyboard_stack_ready()) {
        for (int i = 0; i < 50 && !ble_hs_synced(); i++) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        return ble_hs_synced() ? ESP_OK : ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_hid_gap_init(HIDD_BLE_MODE);
    if (ret != ESP_OK) {
        return ret;
    }

    ble_store_config_init();
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ret = esp_nimble_enable(ble_host_task);
    if (ret != ESP_OK) {
        return ret;
    }

    for (int i = 0; i < 100 && !ble_hs_synced(); i++) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (!ble_hs_synced()) {
        return ESP_ERR_TIMEOUT;
    }

    ble_hid_keyboard_mark_stack_ready();
    return ESP_OK;
}

esp_err_t ble_clone_start_scan(void)
{
    esp_err_t er = ble_clone_ensure_stack();
    if (er != ESP_OK) {
        return er;
    }

    ble_clone_stop();
    (void)ble_gap_adv_stop();
    (void)ble_gap_disc_cancel();

    lock();
    s_count = 0;
    s_list_gen++;
    unlock();

    uint8_t own_addr_type;
    ble_hs_util_ensure_addr(0);
    ble_hs_id_infer_auto(0, &own_addr_type);

    struct ble_gap_disc_params params;
    memset(&params, 0, sizeof(params));
    params.passive = 0; /* active — need scan rsp for names */
    params.filter_duplicates = 0;
    params.limited = 0;
    params.filter_policy = BLE_HCI_SCAN_FILT_NO_WL;
    params.itvl = BLE_GAP_SCAN_ITVL_MS(50);
    params.window = BLE_GAP_SCAN_WIN_MS(40); /* high duty for names */

    s_scanning = true;
    s_scan_rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &params, on_gap, NULL);
    if (s_scan_rc != 0) {
        s_scanning = false;
        ESP_LOGW(TAG, "disc start rc=%d", s_scan_rc);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "BLE scan started");
    return ESP_OK;
}

void ble_clone_stop_scan(void)
{
    if (!ble_hs_synced()) {
        s_scanning = false;
        return;
    }
    (void)ble_gap_disc_cancel();
    s_scanning = false;
}

bool ble_clone_is_scanning(void)
{
    return s_scanning;
}

int ble_clone_device_count(void)
{
    lock();
    int n = s_count;
    unlock();
    return n;
}

bool ble_clone_device_get(int index, ble_clone_dev_t *out)
{
    if (!out) {
        return false;
    }
    lock();
    if (index < 0 || index >= s_count) {
        unlock();
        return false;
    }
    *out = s_devs[index];
    unlock();
    return true;
}

static int build_clone_adv(const ble_clone_dev_t *dev, uint8_t *out)
{
    /* Always advertise a readable Complete Local Name so scanners show text */
    const char *n = (dev->named && dev->name[0]) ? dev->name :
                    (dev->name[0] ? dev->name : "CLONE");
    int nlen = (int)strlen(n);
    if (nlen > 18) {
        nlen = 18;
    }

    int i = 0;
    out[i++] = 0x02;
    out[i++] = 0x01;
    out[i++] = 0x06;
    out[i++] = (uint8_t)(1 + nlen);
    out[i++] = 0x09;
    memcpy(&out[i], n, nlen);
    i += nlen;

    /* Append manufacturer data from capture if room */
    if (dev->adv_len > 0) {
        for (int j = 0; j + 1 < dev->adv_len;) {
            uint8_t flen = dev->adv[j];
            if (flen == 0 || j + 1 + flen > dev->adv_len) {
                break;
            }
            uint8_t type = dev->adv[j + 1];
            if (type == 0xFF && i + 1 + flen <= 31) {
                memcpy(&out[i], &dev->adv[j], 1 + flen);
                i += 1 + flen;
                break;
            }
            j += 1 + flen;
        }
    }
    return i;
}

esp_err_t ble_clone_start(int index)
{
    if (!ble_hs_synced()) {
        return ESP_ERR_INVALID_STATE;
    }

    lock();
    if (index < 0 || index >= s_count) {
        unlock();
        return ESP_ERR_INVALID_ARG;
    }
    ble_clone_dev_t snap = s_devs[index];
    unlock();

    ble_clone_stop_scan();
    ble_hid_keyboard_set_want_adv(false);
    ble_hid_keyboard_pause_adv();
    (void)ble_gap_adv_stop();
    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t adv[31];
    int len = build_clone_adv(&snap, adv);

    ble_addr_t rnd;
    if (ble_hs_id_gen_rnd(0, &rnd) == 0) {
        ble_hs_id_set_rnd(rnd.val);
    }

    int rc = ble_gap_adv_set_data(adv, len);
    if (rc != 0) {
        ESP_LOGW(TAG, "adv_set_data rc=%d", rc);
        return ESP_FAIL;
    }

    struct ble_gap_adv_params params;
    memset(&params, 0, sizeof(params));
    params.conn_mode = BLE_GAP_CONN_MODE_NON;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    params.itvl_min = BLE_GAP_ADV_ITVL_MS(30);
    params.itvl_max = BLE_GAP_ADV_ITVL_MS(50);

    rc = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER, &params, on_gap, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "adv_start rc=%d", rc);
        return ESP_FAIL;
    }

    strncpy(s_clone_name, snap.name, sizeof(s_clone_name) - 1);
    s_clone_name[sizeof(s_clone_name) - 1] = 0;
    s_cloning = true;
    ESP_LOGI(TAG, "cloning '%s' adv_len=%d rssi_was=%d", s_clone_name, len, snap.rssi);
    return ESP_OK;
}

void ble_clone_stop(void)
{
    if (!s_cloning) {
        return;
    }
    s_cloning = false;
    s_clone_name[0] = 0;
    if (ble_hs_synced()) {
        (void)ble_gap_adv_stop();
    }
    ble_hid_keyboard_resume_adv_if_active();
}

bool ble_clone_is_active(void)
{
    return s_cloning;
}

const char *ble_clone_active_name(void)
{
    return s_cloning ? s_clone_name : "";
}

uint32_t ble_clone_list_gen(void)
{
    return s_list_gen;
}

#else /* !NIMBLE */

esp_err_t ble_clone_ensure_stack(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t ble_clone_start_scan(void) { return ESP_ERR_NOT_SUPPORTED; }
void ble_clone_stop_scan(void) {}
bool ble_clone_is_scanning(void) { return false; }
int ble_clone_device_count(void) { return 0; }
bool ble_clone_device_get(int index, ble_clone_dev_t *out) { (void)index; (void)out; return false; }
esp_err_t ble_clone_start(int index) { (void)index; return ESP_ERR_NOT_SUPPORTED; }
void ble_clone_stop(void) {}
bool ble_clone_is_active(void) { return false; }
const char *ble_clone_active_name(void) { return ""; }
uint32_t ble_clone_list_gen(void) { return 0; }

#endif
