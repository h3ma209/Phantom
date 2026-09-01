/**
 * Low-level blit helpers on the active LCD panel.
 *
 * Colors are host-order RGB565; bytes swapped on write (endian for panel).
 * Prefer keyed/partial draws in UI to avoid full-screen flicker.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_lcd_panel_ops.h"

#define LCD_W 320
#define LCD_H 240

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

void display_draw_init(esp_lcd_panel_handle_t panel);
void display_draw_fill_screen(uint16_t color);
void display_draw_fill_rect(int x, int y, int w, int h, uint16_t color);
void display_draw_rect(int x, int y, int w, int h, uint16_t color);
void display_draw_rect_sketch(int x, int y, int w, int h, uint16_t color);

void display_draw_string(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale);
/** Glyph pixels only — leave background. */
void display_draw_string_fg(int x, int y, const char *s, uint16_t fg, int scale);
/** Denser 6×6 glyphs (sampled from 8×8). */
void display_draw_string6(int x, int y, const char *s, uint16_t fg, uint16_t bg);
void display_draw_string6_fg(int x, int y, const char *s, uint16_t fg);
int display_draw_string_width(const char *s, int scale);
int display_draw_string6_width(const char *s);

void display_draw_icon(int x, int y, ui_icon_t icon, uint16_t fg, uint16_t bg);
void display_draw_icon_scaled(int x, int y, ui_icon_t icon, uint16_t fg, int scale);

void display_draw_image_rgb565(int x, int y, int w, int h, const uint16_t *img);
/** Skip pixels equal to `key` (usually COL_BLACK for transparent icons). */
void display_draw_image_key(int x, int y, int w, int h, const uint16_t *img, uint16_t key);
