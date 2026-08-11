#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_lcd_panel_ops.h"

#define LCD_W 320
#define LCD_H 240

/* RGB565 host order (swapped on blit). */
#define COL_BLACK   0x0000
#define COL_RED     0xF800
#define COL_WHITE   0xFFFF
#define COL_GREEN   0x07E0
#define COL_DIM     0x4208
#define COL_PANEL   0x1082
#define COL_SELECT  0x0320

typedef enum {
    ICON_WIFI = 0,
    ICON_RADAR,
    ICON_CHIP,
    ICON_GEAR,
    ICON_LIST,
    ICON_SHIELD,
    ICON_INFO,
    ICON_POWER,
    ICON_COUNT
} ui_icon_t;

void gfx_init(esp_lcd_panel_handle_t panel);
void gfx_fill_screen(uint16_t color);
void gfx_fill_rect(int x, int y, int w, int h, uint16_t color);
void gfx_draw_string(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale);
void gfx_draw_icon(int x, int y, ui_icon_t icon, uint16_t fg, uint16_t bg);
/* host-order RGB565 image (bytes swapped on blit), DMA-safe */
void gfx_draw_image_rgb565(int x, int y, int w, int h, const uint16_t *img);
