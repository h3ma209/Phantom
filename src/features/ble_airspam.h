#pragma once

#include "esp_err.h"
#include <stdbool.h>

esp_err_t ble_airspam_start(void);
void ble_airspam_stop(void);
bool ble_airspam_is_active(void);
