/**
 * ILI9341 bring-up (SPI) and global panel handle for draw layer.
 * Landscape 320×240 after swap_xy / mirror in init.
 */
#pragma once

#include "esp_lcd_panel_ops.h"

void lcd_panel_init(void);
esp_lcd_panel_handle_t lcd_panel_handle(void);
