/**
 * STA scan cache — filled only via wifi_manager.
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define WIFI_SCAN_MAX 16
#define WIFI_SSID_MAX 33

typedef struct {
    char ssid[WIFI_SSID_MAX];
    uint8_t bssid[6];
    int8_t rssi;
    uint8_t channel;
    uint8_t authmode;
} wifi_ap_info_t;

void wifi_scan_clear(void);
int wifi_scan_count(void);
bool wifi_scan_get(int index, wifi_ap_info_t *out);
void wifi_scan_abort(void);
/** Blocking scan (legacy). */
esp_err_t wifi_scan_run(void);
/** Non-blocking scan — finish via wifi_scan_collect() after SCAN_DONE. */
esp_err_t wifi_scan_begin(void);
esp_err_t wifi_scan_collect(void);
bool wifi_scan_busy(void);
