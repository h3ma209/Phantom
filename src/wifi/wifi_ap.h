#pragma once

#include "esp_err.h"
#include <stdbool.h>

esp_err_t wifi_ap_start(const char *ssid, const char *html);
void wifi_ap_stop(void);
void wifi_ap_poll(void);
bool wifi_ap_is_running(void);
const char *wifi_ap_ip_str(void);
