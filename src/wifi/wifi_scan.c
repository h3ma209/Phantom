#include "wifi_scan.h"

#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"

static const char *TAG = "wifi_scan";

static wifi_ap_info_t s_aps[WIFI_SCAN_MAX];
static int s_count;
static bool s_busy;

void wifi_scan_clear(void)
{
    s_count = 0;
    memset(s_aps, 0, sizeof(s_aps));
}

bool wifi_scan_busy(void)
{
    return s_busy;
}

void wifi_scan_abort(void)
{
    if (s_busy) {
        (void)esp_wifi_scan_stop();
        s_busy = false;
    }
}

int wifi_scan_count(void)
{
    return s_count;
}

bool wifi_scan_get(int index, wifi_ap_info_t *out)
{
    if (!out || index < 0 || index >= s_count) {
        return false;
    }
    *out = s_aps[index];
    return true;
}

static esp_err_t fill_from_records(void)
{
    uint16_t n = WIFI_SCAN_MAX;
    wifi_ap_record_t rec[WIFI_SCAN_MAX];
    esp_err_t err = esp_wifi_scan_get_ap_records(&n, rec);
    if (err != ESP_OK) {
        return err;
    }

    s_count = 0;
    for (uint16_t i = 0; i < n && s_count < WIFI_SCAN_MAX; i++) {
        wifi_ap_info_t *d = &s_aps[s_count];
        memset(d, 0, sizeof(*d));
        strncpy(d->ssid, (const char *)rec[i].ssid, WIFI_SSID_MAX - 1);
        memcpy(d->bssid, rec[i].bssid, 6);
        d->rssi = rec[i].rssi;
        d->channel = rec[i].primary;
        d->authmode = (uint8_t)rec[i].authmode;
        if (d->ssid[0] == 0) {
            strncpy(d->ssid, "(hidden)", WIFI_SSID_MAX - 1);
        }
        s_count++;
    }

    ESP_LOGI(TAG, "found %d APs", s_count);
    return ESP_OK;
}

esp_err_t wifi_scan_begin(void)
{
    wifi_scan_clear();

    wifi_scan_config_t cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300,
    };

    esp_err_t err = esp_wifi_scan_start(&cfg, false);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "scan_start %s", esp_err_to_name(err));
        s_busy = false;
        return err;
    }

    s_busy = true;
    return ESP_OK;
}

esp_err_t wifi_scan_collect(void)
{
    if (!s_busy) {
        return ESP_OK;
    }

    esp_err_t err = fill_from_records();
    s_busy = false;
    return err;
}

esp_err_t wifi_scan_run(void)
{
    wifi_scan_clear();

    wifi_scan_config_t cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300,
    };

    esp_err_t err = esp_wifi_scan_start(&cfg, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "scan_start %s", esp_err_to_name(err));
        return err;
    }

    s_busy = false;
    return fill_from_records();
}
