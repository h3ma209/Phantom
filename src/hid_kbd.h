#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t hid_kbd_start(void);
bool hid_kbd_is_connected(void);
void hid_kbd_type(const char *text);
