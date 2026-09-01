#pragma once

#include "esp_err.h"
#include <stdint.h>

/** answer octets are the A-record IPv4, first octet first (e.g. 172,217,28,1). */
esp_err_t wifi_dns_start(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
void wifi_dns_stop(void);
void wifi_dns_poll(void);
