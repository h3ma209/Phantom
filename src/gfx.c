#include "gfx.h"
#include <string.h>
#include "esp_log.h"

/* 8x8 font, ASCII 32..127 — bit0 = left column */
static const uint8_t FONT8[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* sp */
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, /* ! */
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00},
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00},
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00},
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00},
    {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00},
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00},
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00},
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00},
    {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06},
    {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00},
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00},
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, /* 0 */
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00},
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00},
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00},
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00},
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00},
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00},
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00},
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00},
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00},
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00},
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06},
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00},
    {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00},
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00},
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00},
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00},
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, /* A */
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00},
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00},
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00},
    {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00},
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00},
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00},
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00},
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00},
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00},
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00},
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00},
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00},
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00},
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00},
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00},
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00},
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00},
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00},
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00},
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00},
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00},
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00},
    {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00},
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00},
    {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00},
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF},
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, /* a */
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00},
    {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00},
    {0x38,0x30,0x30,0x3e,0x33,0x33,0x6E,0x00},
    {0x00,0x00,0x1E,0x33,0x3f,0x03,0x1E,0x00},
    {0x1C,0x36,0x06,0x0f,0x06,0x06,0x0F,0x00},
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F},
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00},
    {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E},
    {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00},
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00},
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00},
    {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00},
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F},
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78},
    {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00},
    {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00},
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00},
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00},
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00},
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00},
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00},
    {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F},
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00},
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00},
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00},
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00},
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
};

static esp_lcd_panel_handle_t s_panel;
static uint16_t s_line[LCD_W];
static uint16_t s_icon[24 * 24];

static inline uint16_t swap565(uint16_t c)
{
    return (uint16_t)((c >> 8) | (c << 8));
}

void gfx_init(esp_lcd_panel_handle_t panel)
{
    s_panel = panel;
}

void gfx_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > LCD_W) {
        w = LCD_W - x;
    }
    if (y + h > LCD_H) {
        h = LCD_H - y;
    }
    if (w <= 0 || h <= 0) {
        return;
    }

    uint16_t c = swap565(color);
    for (int i = 0; i < w; i++) {
        s_line[i] = c;
    }
    for (int row = 0; row < h; row++) {
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel, x, y + row, x + w, y + row + 1, s_line));
    }
}

void gfx_fill_screen(uint16_t color)
{
    gfx_fill_rect(0, 0, LCD_W, LCD_H, color);
}

int gfx_string_width(const char *s, int scale)
{
    if (!s || scale < 1) {
        return 0;
    }
    int len = 0;
    while (s[len]) {
        len++;
    }
    return len * 8 * scale;
}

void gfx_draw_string(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale)
{
    if (!s || scale < 1) {
        return;
    }
    int len = 0;
    while (s[len]) {
        len++;
    }
    if (len == 0) {
        return;
    }

    const int cw = 8 * scale;
    const int ch = 8 * scale;
    int max_chars = (LCD_W - x) / cw;
    if (max_chars <= 0) {
        return;
    }
    if (len > max_chars) {
        len = max_chars;
    }
    const int tw = len * cw;
    uint16_t f = swap565(fg);
    uint16_t b = swap565(bg);

    for (int row = 0; row < ch; row++) {
        int glyph_row = row / scale;
        for (int ci = 0; ci < len; ci++) {
            char chv = s[ci];
            if (chv < 32 || chv > 127) {
                chv = '?';
            }
            uint8_t bits = FONT8[chv - 32][glyph_row];
            for (int col = 0; col < 8; col++) {
                uint16_t pix = (bits & (1 << col)) ? f : b;
                for (int t = 0; t < scale; t++) {
                    s_line[ci * cw + col * scale + t] = pix;
                }
            }
        }
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel, x, y + row, x + tw, y + row + 1, s_line));
    }
}

void gfx_draw_string_fg(int x, int y, const char *s, uint16_t fg, int scale)
{
    if (!s || scale < 1) {
        return;
    }
    uint16_t f = swap565(fg);
    int cx = x;
    for (const char *p = s; *p; p++) {
        char chv = *p;
        if (chv < 32 || chv > 127) {
            chv = '?';
        }
        const uint8_t *glyph = FONT8[chv - 32];
        for (int row = 0; row < 8; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < 8; col++) {
                if (!(bits & (1 << col))) {
                    continue;
                }
                int px = cx + col * scale;
                int py = y + row * scale;
                if (px < 0 || py < 0 || px >= LCD_W || py >= LCD_H) {
                    continue;
                }
                int rw = scale;
                int rh = scale;
                if (px + rw > LCD_W) {
                    rw = LCD_W - px;
                }
                if (py + rh > LCD_H) {
                    rh = LCD_H - py;
                }
                for (int i = 0; i < rw; i++) {
                    s_line[i] = f;
                }
                for (int t = 0; t < rh; t++) {
                    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel, px, py + t, px + rw, py + t + 1, s_line));
                }
            }
        }
        cx += 8 * scale;
        if (cx >= LCD_W) {
            break;
        }
    }
}

static void plot(uint16_t *buf, int w, int px, int py, uint16_t c)
{
    if (px >= 0 && py >= 0 && px < w && py < w) {
        buf[py * w + px] = c;
    }
}

static void render_icon(ui_icon_t icon, uint16_t fg, uint16_t bg)
{
    enum { S = 24 };
    uint16_t c = swap565(fg);
    uint16_t z = swap565(bg);
    for (int i = 0; i < S * S; i++) {
        s_icon[i] = z;
    }

    switch (icon) {
    case ICON_WIFI:
        for (int r = 3; r <= 9; r += 3) {
            for (int dx = -r; dx <= r; dx++) {
                for (int dy = -r; dy <= 0; dy++) {
                    int d2 = dx * dx + dy * dy;
                    if (d2 >= (r - 1) * (r - 1) && d2 <= r * r) {
                        plot(s_icon, S, 12 + dx, 14 + dy, c);
                    }
                }
            }
        }
        plot(s_icon, S, 12, 17, c);
        plot(s_icon, S, 11, 18, c);
        plot(s_icon, S, 12, 18, c);
        plot(s_icon, S, 13, 18, c);
        break;
    case ICON_RADAR:
        for (int r = 3; r <= 10; r += 3) {
            for (int dx = -r; dx <= r; dx++) {
                for (int dy = -r; dy <= r; dy++) {
                    int d2 = dx * dx + dy * dy;
                    if (d2 >= (r - 1) * (r - 1) && d2 <= r * r) {
                        plot(s_icon, S, 12 + dx, 12 + dy, c);
                    }
                }
            }
        }
        for (int i = 0; i < 10; i++) {
            plot(s_icon, S, 12 + i, 12 - i / 2, c);
        }
        break;
    case ICON_CHIP:
        for (int i = 6; i < 18; i++) {
            for (int j = 6; j < 18; j++) {
                if (i == 6 || i == 17 || j == 6 || j == 17) {
                    plot(s_icon, S, i, j, c);
                }
            }
        }
        for (int i = 4; i < 20; i += 3) {
            plot(s_icon, S, i, 4, c);
            plot(s_icon, S, i, 5, c);
            plot(s_icon, S, i, 19, c);
            plot(s_icon, S, i, 20, c);
            plot(s_icon, S, 4, i, c);
            plot(s_icon, S, 5, i, c);
            plot(s_icon, S, 19, i, c);
            plot(s_icon, S, 20, i, c);
        }
        break;
    case ICON_GEAR:
        for (int dx = -8; dx <= 8; dx++) {
            for (int dy = -8; dy <= 8; dy++) {
                int d2 = dx * dx + dy * dy;
                if ((d2 >= 25 && d2 <= 49) || (d2 <= 9)) {
                    plot(s_icon, S, 12 + dx, 12 + dy, c);
                }
                if ((dx == 0 || dy == 0 || dx == dy || dx == -dy) && d2 <= 64 && d2 >= 36) {
                    plot(s_icon, S, 12 + dx, 12 + dy, c);
                }
            }
        }
        break;
    case ICON_LIST:
        for (int row = 0; row < 4; row++) {
            int yy = 4 + row * 5;
            for (int xx = 4; xx < 8; xx++) {
                plot(s_icon, S, xx, yy, c);
                plot(s_icon, S, xx, yy + 1, c);
            }
            for (int xx = 10; xx < 20; xx++) {
                plot(s_icon, S, xx, yy, c);
                plot(s_icon, S, xx, yy + 1, c);
            }
        }
        break;
    case ICON_SHIELD:
        for (int yy = 3; yy < 14; yy++) {
            int half = (yy < 8) ? (yy - 2) : (16 - yy);
            for (int xx = 12 - half; xx <= 12 + half; xx++) {
                if (yy == 3 || xx == 12 - half || xx == 12 + half) {
                    plot(s_icon, S, xx, yy, c);
                }
            }
        }
        for (int yy = 14; yy < 21; yy++) {
            int half = 21 - yy;
            for (int xx = 12 - half; xx <= 12 + half; xx++) {
                if (xx == 12 - half || xx == 12 + half || yy == 20) {
                    plot(s_icon, S, xx, yy, c);
                }
            }
        }
        break;
    case ICON_INFO:
        for (int dx = -8; dx <= 8; dx++) {
            for (int dy = -8; dy <= 8; dy++) {
                int d2 = dx * dx + dy * dy;
                if (d2 >= 49 && d2 <= 64) {
                    plot(s_icon, S, 12 + dx, 12 + dy, c);
                }
            }
        }
        plot(s_icon, S, 12, 7, c);
        plot(s_icon, S, 12, 8, c);
        for (int yy = 11; yy < 18; yy++) {
            plot(s_icon, S, 12, yy, c);
        }
        break;
    case ICON_POWER:
        for (int dx = -7; dx <= 7; dx++) {
            for (int dy = -7; dy <= 7; dy++) {
                int d2 = dx * dx + dy * dy;
                if (d2 >= 36 && d2 <= 49 && !(dy < -2 && dx > -3 && dx < 3)) {
                    plot(s_icon, S, 12 + dx, 13 + dy, c);
                }
            }
        }
        for (int yy = 4; yy < 14; yy++) {
            plot(s_icon, S, 12, yy, c);
            plot(s_icon, S, 11, yy, c);
            plot(s_icon, S, 13, yy, c);
        }
        break;
    default:
        break;
    }
}

void gfx_draw_icon(int x, int y, ui_icon_t icon, uint16_t fg, uint16_t bg)
{
    enum { S = 24 };
    render_icon(icon, fg, bg);
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel, x, y, x + S, y + S, s_icon));
}

void gfx_draw_icon_scaled(int x, int y, ui_icon_t icon, uint16_t fg, int scale)
{
    enum { S = 24 };
    if (scale < 1) {
        return;
    }
    /* magenta key so only fg pixels blit */
    render_icon(icon, fg, 0xF81F);
    const uint16_t key = swap565(0xF81F);
    const uint16_t f = swap565(fg);
    const int out = S * scale;
    for (int row = 0; row < out; row++) {
        int sy = row / scale;
        int run_x = -1;
        int run_w = 0;
        for (int col = 0; col < out; col++) {
            int sx = col / scale;
            uint16_t p = s_icon[sy * S + sx];
            if (p == key) {
                if (run_w > 0) {
                    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(
                        s_panel, x + run_x, y + row, x + run_x + run_w, y + row + 1, s_line));
                    run_w = 0;
                    run_x = -1;
                }
                continue;
            }
            if (run_w == 0) {
                run_x = col;
            }
            s_line[run_w++] = f;
        }
        if (run_w > 0) {
            ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(
                s_panel, x + run_x, y + row, x + run_x + run_w, y + row + 1, s_line));
        }
    }
}

void gfx_draw_image_rgb565(int x, int y, int w, int h, const uint16_t *img)
{
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            uint16_t p = img[row * w + col];
            s_line[col] = (uint16_t)((p >> 8) | (p << 8));
        }
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel, x, y + row, x + w, y + row + 1, s_line));
    }
}

void gfx_draw_image_key(int x, int y, int w, int h, const uint16_t *img, uint16_t key)
{
    for (int row = 0; row < h; row++) {
        int run_x = -1;
        int run_w = 0;
        for (int col = 0; col < w; col++) {
            uint16_t p = img[row * w + col];
            if (p == key) {
                if (run_w > 0) {
                    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(
                        s_panel, x + run_x, y + row, x + run_x + run_w, y + row + 1, s_line));
                    run_w = 0;
                    run_x = -1;
                }
                continue;
            }
            if (run_w == 0) {
                run_x = col;
            }
            s_line[run_w++] = (uint16_t)((p >> 8) | (p << 8));
        }
        if (run_w > 0) {
            ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(
                s_panel, x + run_x, y + row, x + run_x + run_w, y + row + 1, s_line));
        }
    }
}
