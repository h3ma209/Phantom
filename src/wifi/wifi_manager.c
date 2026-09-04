#include "wifi_manager.h"

#include <string.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_ap.h"
#include "wifi_portals.h"

static const char *TAG = "wifi_mgr";

static wifi_mgr_mode_t s_mode = WIFI_MGR_OFF;
static bool s_inited;
static bool s_scan_done;
static char s_ssid[WIFI_SSID_MAX] = "FakeAP";
static bool s_ssid_cloned;
static int s_portal;
static esp_netif_t *s_sta_netif;

static void on_wifi_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        s_scan_done = true;
    }
}

static void apply_wifi_power_limits(void)
{
    /* 8 dBm (32 × 0.25 dBm) — less peak current on weak USB + TFT */
    (void)esp_wifi_set_max_tx_power(32);
}

void wifi_manager_apply_power_limits(void)
{
    apply_wifi_power_limits();
}

static esp_err_t ensure_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "nvs erase %s", esp_err_to_name(err));
            return err;
        }
        err = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(err, TAG, "nvs");

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    if (!s_sta_netif) {
        s_sta_netif = esp_netif_create_default_wifi_sta();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE,
                                              on_wifi_event, NULL, NULL);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    s_inited = true;
    ESP_LOGI(TAG, "wifi stack ready");
    return ESP_OK;
}

esp_err_t wifi_manager_init(void)
{
    return ensure_init();
}

static void leave_mode(void)
{
    if (s_mode == WIFI_MGR_AP) {
        wifi_ap_stop();
    } else if (s_mode == WIFI_MGR_SCAN) {
        wifi_scan_abort();
        (void)esp_wifi_stop();
    }
    s_mode = WIFI_MGR_OFF;
}

esp_err_t wifi_manager_set_mode(wifi_mgr_mode_t mode)
{
    ESP_RETURN_ON_ERROR(ensure_init(), TAG, "init");

    if (mode == s_mode) {
        if (mode == WIFI_MGR_SCAN) {
            esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
            if (err != ESP_OK) {
                return err;
            }
            err = esp_wifi_start();
            if (err != ESP_OK && err != ESP_ERR_WIFI_CONN) {
                return err;
            }
            apply_wifi_power_limits();
            s_scan_done = false;
            return wifi_scan_begin();
        }
        return ESP_OK;
    }

    leave_mode();

    if (mode == WIFI_MGR_OFF) {
        return ESP_OK;
    }

    if (mode == WIFI_MGR_SCAN) {
        esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
        if (err != ESP_OK) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
        err = esp_wifi_start();
        if (err != ESP_OK && err != ESP_ERR_WIFI_CONN) {
            return err;
        }
        apply_wifi_power_limits();
        s_scan_done = false;
        err = wifi_scan_begin();
        if (err == ESP_OK) {
            s_mode = WIFI_MGR_SCAN;
        }
        return err;
    }

    /* WIFI_MGR_AP */
    esp_err_t err = wifi_ap_start(s_ssid, wifi_portal_html(s_portal));
    if (err == ESP_OK) {
        s_mode = WIFI_MGR_AP;
    }
    return err;
}

wifi_mgr_mode_t wifi_manager_mode(void)
{
    return s_mode;
}

void wifi_manager_tick(void)
{
    if (s_scan_done && wifi_scan_busy()) {
        s_scan_done = false;
        if (wifi_scan_collect() != ESP_OK) {
            ESP_LOGW(TAG, "scan collect failed");
        }
    }

    if (s_mode == WIFI_MGR_AP) {
        wifi_ap_poll();
    }
}

esp_err_t wifi_manager_begin_scan(void)
{
    return wifi_manager_set_mode(WIFI_MGR_SCAN);
}

bool wifi_manager_scan_busy(void)
{
    return wifi_scan_busy();
}

int wifi_manager_ap_count(void)
{
    return wifi_scan_count();
}

bool wifi_manager_ap_get(int i, wifi_ap_info_t *out)
{
    return wifi_scan_get(i, out);
}

esp_err_t wifi_manager_clone_ssid(int scan_index)
{
    wifi_ap_info_t ap;
    if (!wifi_scan_get(scan_index, &ap)) {
        return ESP_ERR_INVALID_ARG;
    }
    strncpy(s_ssid, ap.ssid, sizeof(s_ssid) - 1);
    s_ssid[sizeof(s_ssid) - 1] = 0;
    s_ssid_cloned = true;
    ESP_LOGI(TAG, "clone SSID -> %s", s_ssid);
    return ESP_OK;
}

void wifi_manager_set_portal(int index)
{
    if (index < 0) {
        index = 0;
    }
    if (index >= WIFI_PORTAL_COUNT) {
        index = WIFI_PORTAL_COUNT - 1;
    }
    s_portal = index;
}

int wifi_manager_portal(void)
{
    return s_portal;
}

const char *wifi_manager_ssid(void)
{
    return s_ssid;
}

bool wifi_manager_ssid_cloned(void)
{
    return s_ssid_cloned;
}

bool wifi_manager_ap_running(void)
{
    return s_mode == WIFI_MGR_AP && wifi_ap_is_running();
}

const char *wifi_manager_ip(void)
{
    return wifi_ap_ip_str();
}
