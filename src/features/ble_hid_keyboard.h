/**
 * BLE HID keyboard peripheral (`DEDSEC KBD`).
 *
 * Owns NimBLE host bring-up for the device. Apple Spam and other BLE features
 * must go through the shared stack helpers below — do not init NimBLE twice.
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>

esp_err_t ble_hid_keyboard_start(void);
bool ble_hid_keyboard_is_connected(void);
void ble_hid_keyboard_type(const char *text);

/* --- Shared NimBLE stack (HID + Apple Spam) --- */

bool ble_hid_keyboard_stack_ready(void);
void ble_hid_keyboard_mark_stack_ready(void);

/** Pause connectable HID advertising (e.g. before Apple Spam). */
void ble_hid_keyboard_pause_adv(void);
/** Resume HID adv if user still has keyboard mode armed. */
void ble_hid_keyboard_resume_adv_if_active(void);

bool ble_hid_keyboard_want_adv(void);
void ble_hid_keyboard_set_want_adv(bool on);
