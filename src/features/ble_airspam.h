/**
 * Apple Continuity / "AppleJuice" style advertising spam.
 *
 * Sends non-connectable manufacturer payloads that can trigger iOS popups.
 * Does NOT appear in Bluetooth device scan lists — test with iPhone BT on.
 *
 * Requires NimBLE stack already up (via HID start or ensure_stack path).
 * Mutually exclusive with HID advertising while active.
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>

esp_err_t ble_airspam_start(void);
void ble_airspam_stop(void);
bool ble_airspam_is_active(void);
