/**
 * Sole owner of esp_wifi mode. UI talks only to this.
 *
 * Named wifi_mgr_mode_t — ESP-IDF already owns wifi_mode_t.
 *
 * WIFI_MGR_OFF  — radio idle
 * WIFI_MGR_SCAN — STA scan into cache
 * WIFI_MGR_AP   — soft AP + DNS hijack + captive HTTP
 */
#pragma once

#include "esp_err.h"
#include "wifi_scan.h"
#include <stdbool.h>

typedef enum {
    WIFI_MGR_OFF = 0,
    WIFI_MGR_SCAN,
    WIFI_MGR_AP,
} wifi_mgr_mode_t;

esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_set_mode(wifi_mgr_mode_t mode);
wifi_mgr_mode_t wifi_manager_mode(void);

/** Service DNS while AP is up; finish async WiFi scan. Call from nav loop. */
void wifi_manager_tick(void);

/** Start non-blocking STA scan (Clone AP list). */
esp_err_t wifi_manager_begin_scan(void);
bool wifi_manager_scan_busy(void);

int wifi_manager_ap_count(void);
bool wifi_manager_ap_get(int i, wifi_ap_info_t *out);
esp_err_t wifi_manager_clone_ssid(int scan_index);

void wifi_manager_set_portal(int index);
int wifi_manager_portal(void);

const char *wifi_manager_ssid(void);
bool wifi_manager_ssid_cloned(void);
bool wifi_manager_ap_running(void);
const char *wifi_manager_ip(void);

/** Cap WiFi TX power after esp_wifi_start (weak USB + TFT). */
void wifi_manager_apply_power_limits(void);
