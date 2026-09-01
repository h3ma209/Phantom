#include "wifi_ap.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_netif_ip_addr.h"
#include "wifi_dns.h"
#include "wifi_manager.h"

static const char *TAG = "wifi_ap";

#define AP_IP_A 172
#define AP_IP_B 217
#define AP_IP_C 28
#define AP_IP_D 1

static httpd_handle_t s_httpd;
static const char *s_html;
static bool s_running;
static char s_ip[16];
static esp_netif_t *s_ap_netif;

static esp_err_t portal_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "-1");
    httpd_resp_set_hdr(req, "Connection", "close");
    const char *html = s_html ? s_html : "<html><body>portal</body></html>";
    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

static void register_uri(httpd_handle_t hd, const char *uri)
{
    httpd_uri_t u = {
        .uri = uri,
        .method = HTTP_GET,
        .handler = portal_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(hd, &u);
}

static esp_err_t start_httpd(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.max_uri_handlers = 16;
    cfg.lru_purge_enable = true;

    if (httpd_start(&s_httpd, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start");
        return ESP_FAIL;
    }

    register_uri(s_httpd, "/");
    register_uri(s_httpd, "/generate_204");
    register_uri(s_httpd, "/gen_204");
    register_uri(s_httpd, "/hotspot-detect.html");
    register_uri(s_httpd, "/library/test/success.html");
    register_uri(s_httpd, "/connecttest.txt");
    register_uri(s_httpd, "/ncsi.txt");
    register_uri(s_httpd, "/fwlink");
    register_uri(s_httpd, "/redirect");
    register_uri(s_httpd, "/mobile/status.php");
    register_uri(s_httpd, "/check_network_status.txt");
    register_uri(s_httpd, "/*");
    return ESP_OK;
}

esp_err_t wifi_ap_start(const char *ssid, const char *html)
{
    if (s_running) {
        return ESP_OK;
    }

    s_html = html;
    if (!s_ap_netif) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
    }

    wifi_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy((char *)cfg.ap.ssid, ssid && ssid[0] ? ssid : "FakeAP", sizeof(cfg.ap.ssid) - 1);
    cfg.ap.ssid_len = (uint8_t)strlen((char *)cfg.ap.ssid);
    cfg.ap.channel = 1;
    cfg.ap.max_connection = 4;
    cfg.ap.authmode = WIFI_AUTH_OPEN;
    cfg.ap.ssid_hidden = 0;

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_wifi_set_config(WIFI_IF_AP, &cfg);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_WIFI_CONN) {
        return err;
    }
    wifi_manager_apply_power_limits();

    esp_netif_ip_info_t ipinfo;
    memset(&ipinfo, 0, sizeof(ipinfo));
    esp_netif_set_ip4_addr(&ipinfo.ip, AP_IP_A, AP_IP_B, AP_IP_C, AP_IP_D);
    esp_netif_set_ip4_addr(&ipinfo.gw, AP_IP_A, AP_IP_B, AP_IP_C, AP_IP_D);
    esp_netif_set_ip4_addr(&ipinfo.netmask, 255, 255, 255, 0);
    (void)esp_netif_dhcps_stop(s_ap_netif);
    err = esp_netif_set_ip_info(s_ap_netif, &ipinfo);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_ip_info %s", esp_err_to_name(err));
        (void)esp_wifi_stop();
        return err;
    }
    err = esp_netif_dhcps_start(s_ap_netif);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "dhcps_start %s", esp_err_to_name(err));
        (void)esp_wifi_stop();
        return err;
    }

    snprintf(s_ip, sizeof(s_ip), "%d.%d.%d.%d", AP_IP_A, AP_IP_B, AP_IP_C, AP_IP_D);

    err = wifi_dns_start(AP_IP_A, AP_IP_B, AP_IP_C, AP_IP_D);
    if (err != ESP_OK) {
        (void)esp_wifi_stop();
        return err;
    }
    err = start_httpd();
    if (err != ESP_OK) {
        wifi_dns_stop();
        (void)esp_wifi_stop();
        return err;
    }

    s_running = true;
    ESP_LOGI(TAG, "AP '%s' ip=%s", cfg.ap.ssid, s_ip);
    return ESP_OK;
}

void wifi_ap_stop(void)
{
    if (!s_running) {
        return;
    }
    if (s_httpd) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
    }
    wifi_dns_stop();
    (void)esp_wifi_stop();
    s_running = false;
    ESP_LOGI(TAG, "AP stopped");
}

void wifi_ap_poll(void)
{
    wifi_dns_poll();
    wifi_dns_poll();
}

bool wifi_ap_is_running(void)
{
    return s_running;
}

const char *wifi_ap_ip_str(void)
{
    return s_running ? s_ip : "—";
}
