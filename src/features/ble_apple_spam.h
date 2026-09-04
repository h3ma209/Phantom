/**
 * Apple Spam — Continuity BLE popup flood (AirPods, Apple TV, HomePod, etc.).
 *
 * Rotates MAC + payload every burst so iOS cannot suppress after Cancel.
 * Non-connectable only; does not appear in normal BT scan lists.
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>

esp_err_t ble_apple_spam_start(void);
void ble_apple_spam_stop(void);
bool ble_apple_spam_is_active(void);
