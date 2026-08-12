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
#define COL_PURPLE  0x981F
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
void gfx_draw_rect(int x, int y, int w, int h, uint16_t color);
/* rough sketchy border (offset edges) */
void gfx_draw_rect_sketch(int x, int y, int w, int h, uint16_t color);
void gfx_draw_string(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale);
/* draw only glyph pixels; leave bg untouched */
void gfx_draw_string_fg(int x, int y, const char *s, uint16_t fg, int scale);
/* 6x6 glyphs (sampled from 8x8) — denser UI text */
void gfx_draw_string6(int x, int y, const char *s, uint16_t fg, uint16_t bg);
void gfx_draw_string6_fg(int x, int y, const char *s, uint16_t fg);
int gfx_string_width(const char *s, int scale);
int gfx_string6_width(const char *s);
void gfx_draw_icon(int x, int y, ui_icon_t icon, uint16_t fg, uint16_t bg);
/* nearest-neighbor scale of 24px icon (scale >= 1) */
void gfx_draw_icon_scaled(int x, int y, ui_icon_t icon, uint16_t fg, int scale);
/* host-order RGB565 image (bytes swapped on blit), DMA-safe */
void gfx_draw_image_rgb565(int x, int y, int w, int h, const uint16_t *img);
/* skip pixels equal to key (host RGB565, usually COL_BLACK) */
void gfx_draw_image_key(int x, int y, int w, int h, const uint16_t *img, uint16_t key);
