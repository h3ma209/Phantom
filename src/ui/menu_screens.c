#include "menu_screens.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_ops.h"
#include "display_draw.h"
#include "lcd_panel.h"
#include "menu_catalog.h"
#include "ble_hid_keyboard.h"
#include "ble_airspam.h"
#include "asset_splash.h"
#include "asset_menu_bg.h"
#include "asset_submenu_bg.h"
#include "asset_cursor.h"

#define GLITCH_FRAMES 10
#define GLITCH_FRAME_MS 110

/* Fixed focus slot so scroll only patches one band (no full-screen flash). */
#define FOCUS_ICON_SLOT_W 90
#define FOCUS_ICON_SLOT_H 100
#define FOCUS_GAP         14
#define FOCUS_TITLE_SCALE 2
#define FOCUS_TITLE_H     (8 * FOCUS_TITLE_SCALE)
#define FOCUS_BLOCK_H     (FOCUS_ICON_SLOT_H + FOCUS_GAP + FOCUS_TITLE_H)
#define FOCUS_Y0          ((LCD_H - FOCUS_BLOCK_H) / 2)

#define SUB_LIST_X     14
#define SUB_LIST_Y0    30
#define SUB_ROW_H      22
#define SUB_CURSOR_GAP 4
#define SUB_PANEL_X    190
#define SUB_PANEL_Y    24
#define SUB_PANEL_W    122
#define SUB_PANEL_H    158
#define SUB_FOOTER_H   16
#define SUB_ACTIVE_H   14
#define SUB_LABEL_SCALE 2
#define SUB_LABEL_MAX  10 /* chars at scale 2 before clip */

static uint16_t prng_next(uint16_t *state)
{
    *state = (uint16_t)(*state * 1103515245u + 12345u);
    return *state;
}

void menu_screens_draw_splash(void)
{
    display_draw_image_rgb565(0, 0, ASSET_SPLASH_W, ASSET_SPLASH_H, ASSET_SPLASH_DATA);
}

void menu_screens_splash_glitch_out(void)
{
    esp_lcd_panel_handle_t panel = lcd_panel_handle();
    uint16_t rng = 0xA5F1;
    uint16_t line[LCD_W];

    for (int frame = 0; frame < GLITCH_FRAMES; frame++) {
        for (int band = 0; band < 6; band++) {
            int y0 = prng_next(&rng) % LCD_H;
            int bh = 4 + (prng_next(&rng) % 18);
            if (y0 + bh > LCD_H) {
                bh = LCD_H - y0;
            }
            int shift = (int)(prng_next(&rng) % 41) - 20;
            for (int y = y0; y < y0 + bh; y++) {
                const uint16_t *src = &ASSET_SPLASH_DATA[y * ASSET_SPLASH_W];
                for (int x = 0; x < LCD_W; x++) {
                    int sx = x - shift;
                    if (sx < 0 || sx >= LCD_W || (prng_next(&rng) & 0x3F) == 0) {
                        line[x] = 0;
                    } else {
                        uint16_t p = src[sx];
                        line[x] = (uint16_t)((p >> 8) | (p << 8));
                    }
                }
                ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, y, LCD_W, y + 1, line));
            }
        }

        int blocks = 2 + frame;
        for (int b = 0; b < blocks; b++) {
            int bx = prng_next(&rng) % LCD_W;
            int by = prng_next(&rng) % LCD_H;
            int bw = 8 + (prng_next(&rng) % 48);
            int bh = 4 + (prng_next(&rng) % 24);
            display_draw_fill_rect(bx, by, bw, bh, COL_BLACK);
        }

        vTaskDelay(pdMS_TO_TICKS(GLITCH_FRAME_MS));
    }

    display_draw_fill_screen(COL_BLACK);
}

static void blit_menu_bg_rect(int x0, int y0, int w, int h)
{
    esp_lcd_panel_handle_t panel = lcd_panel_handle();
    if (w <= 0 || h <= 0) {
        return;
    }
    if (x0 < 0) {
        w += x0;
        x0 = 0;
    }
    if (y0 < 0) {
        h += y0;
        y0 = 0;
    }
    if (x0 + w > LCD_W) {
        w = LCD_W - x0;
    }
    if (y0 + h > LCD_H) {
        h = LCD_H - y0;
    }
    if (w <= 0 || h <= 0) {
        return;
    }

    uint16_t line[LCD_W];
    for (int y = y0; y < y0 + h; y++) {
        int ty = y % ASSET_MENU_BG_H;
        for (int x = x0; x < x0 + w; x++) {
            int tx = x % ASSET_MENU_BG_W;
            uint16_t p = ASSET_MENU_BG_DATA[ty * ASSET_MENU_BG_W + tx];
            line[x - x0] = (uint16_t)((p >> 8) | (p << 8));
        }
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, x0, y, x0 + w, y + 1, line));
    }
}

static void draw_menu_background(void)
{
    blit_menu_bg_rect(0, 0, LCD_W, LCD_H);
}

static void draw_submenu_background(void)
{
    display_draw_image_rgb565(0, 0, ASSET_SUBMENU_BG_W, ASSET_SUBMENU_BG_H, ASSET_SUBMENU_BG_DATA);
}

static void draw_focus_item(const menu_item_t *item, bool full_bg)
{
    if (full_bg) {
        draw_menu_background();
    } else {
        blit_menu_bg_rect(0, FOCUS_Y0, LCD_W, FOCUS_BLOCK_H);
    }

    int icon_w = item->img_w > 0 ? item->img_w : (24 * 3);
    int icon_h = item->img_h > 0 ? item->img_h : (24 * 3);
    const int ix = (LCD_W - icon_w) / 2;
    const int iy = FOCUS_Y0 + (FOCUS_ICON_SLOT_H - icon_h) / 2;

    if (item->img && item->img_w > 0 && item->img_h > 0) {
        display_draw_image_key(ix, iy, item->img_w, item->img_h, item->img, COL_BLACK);
    } else {
        display_draw_icon_scaled(ix, iy, ICON_CHIP, COL_WHITE, 3);
    }

    int title_scale = FOCUS_TITLE_SCALE;
    int tw = display_draw_string_width(item->title, title_scale);
    if (tw > LCD_W - 8) {
        title_scale = 1;
        tw = display_draw_string_width(item->title, title_scale);
    }
    int tx = (LCD_W - tw) / 2;
    if (tx < 0) {
        tx = 0;
    }
    display_draw_string_fg(tx, FOCUS_Y0 + FOCUS_ICON_SLOT_H + FOCUS_GAP, item->title, COL_WHITE, title_scale);
}

void menu_screens_draw_main_menu(int selected, bool full_bg)
{
    draw_focus_item(&MENU_MAIN[selected], full_bg);
}

static const char *bt_active_label(bool hid_active, bool airspam_on)
{
    if (airspam_on) {
        return "AirSpam";
    }
    if (hid_active) {
        return "Keyboard";
    }
    return "none";
}

static void draw_panel_field(int x, int y, const char *key, const char *val, uint16_t val_color)
{
    display_draw_string_fg(x, y, key, COL_DIM, 1);
    int max_chars = (SUB_PANEL_W - 16) / 8;
    char truncated[24];
    int n = 0;
    if (val) {
        while (val[n] && n < max_chars && n < (int)sizeof(truncated) - 1) {
            truncated[n] = val[n];
            n++;
        }
    }
    truncated[n] = 0;
    display_draw_string_fg(x, y + 10, truncated, val_color, 1);
}

static void draw_sub_panel(const sub_item_t *item, bool hid_busy)
{
    display_draw_fill_rect(SUB_PANEL_X + 3, SUB_PANEL_Y + 3, SUB_PANEL_W - 6, SUB_PANEL_H - 6, COL_BLACK);
    display_draw_rect_sketch(SUB_PANEL_X, SUB_PANEL_Y, SUB_PANEL_W, SUB_PANEL_H, COL_WHITE);

    const char *status = item->status_idle;
    uint16_t status_col = COL_DIM;
    const char *conn = item->conn_val;
    uint16_t conn_col = COL_RED;

    if (item->action == ACT_HID_KBD) {
        status = hid_busy ? item->status_busy : "idle";
        status_col = hid_busy ? COL_GREEN : COL_DIM;
        if (ble_hid_keyboard_is_connected()) {
            conn = "connected";
            conn_col = COL_GREEN;
        } else {
            conn = "not connected";
            conn_col = COL_RED;
        }
    } else if (item->action == ACT_AIRSPAM) {
        status = ble_airspam_is_active() ? item->status_busy : "idle";
        status_col = ble_airspam_is_active() ? COL_GREEN : COL_DIM;
        conn = ble_airspam_is_active() ? "broadcasting" : "stopped";
        conn_col = ble_airspam_is_active() ? COL_GREEN : COL_DIM;
    } else if (item->action == ACT_SOON) {
        status = item->status_idle;
        status_col = COL_DIM;
        conn_col = COL_DIM;
    } else if (item->action == ACT_BACK) {
        status = "—";
        conn = "—";
        status_col = COL_DIM;
        conn_col = COL_DIM;
    }

    int tx = SUB_PANEL_X + 8;
    int ty = SUB_PANEL_Y + 12;
    int row = 0;

    draw_panel_field(tx, ty + row * 36, "name", item->name_val, COL_PURPLE);
    row++;
    draw_panel_field(tx, ty + row * 36, "status", status, status_col);
    row++;
    draw_panel_field(tx, ty + row * 36, "connection", conn, conn_col);

    if (item->hint && item->action != ACT_BACK) {
        int hx = SUB_PANEL_X + 8;
        int hy = SUB_PANEL_Y + SUB_PANEL_H - 30;
        const char *p = item->hint;
        int line = 0;
        char buf[18];
        int maxc = (SUB_PANEL_W - 16) / 6;
        if (maxc > 17) {
            maxc = 17;
        }
        while (*p && line < 2) {
            int n = 0;
            while (*p && n < maxc) {
                buf[n++] = *p++;
            }
            buf[n] = 0;
            display_draw_string6_fg(hx, hy + line * 8, buf, COL_DIM);
            line++;
        }
    }
}

static void draw_sub_list_row(const category_t *cat, int index, bool selected)
{
    esp_lcd_panel_handle_t panel = lcd_panel_handle();
    const sub_item_t *item = &cat->items[index];
    const int y = SUB_LIST_Y0 + index * SUB_ROW_H;
    const int text_x = SUB_LIST_X + ASSET_CURSOR_W + SUB_CURSOR_GAP;
    const int max_w = SUB_PANEL_X - text_x - 6;

    for (int row = 0; row < SUB_ROW_H; row++) {
        int sy = y + row;
        if (sy < 0 || sy >= LCD_H - SUB_FOOTER_H) {
            continue;
        }
        uint16_t line[LCD_W];
        int x0 = 6;
        int w = SUB_PANEL_X - 10;
        for (int x = 0; x < w; x++) {
            uint16_t p = ASSET_SUBMENU_BG_DATA[sy * ASSET_SUBMENU_BG_W + (x0 + x)];
            line[x] = (uint16_t)((p >> 8) | (p << 8));
        }
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, x0, sy, x0 + w, sy + 1, line));
    }

    char label[24];
    int n = 0;
    while (item->label[n] && n < SUB_LABEL_MAX && n < (int)sizeof(label) - 1) {
        label[n] = item->label[n];
        n++;
    }
    label[n] = 0;

    if (selected) {
        int label_w = display_draw_string_width(label, SUB_LABEL_SCALE);
        if (label_w > max_w) {
            label_w = max_w;
        }
        int box_h = 8 * SUB_LABEL_SCALE + 4;
        int box_w = label_w + 6;
        int box_x = text_x - 2;
        display_draw_fill_rect(box_x, y + 1, box_w, box_h, COL_WHITE);
        display_draw_image_key(SUB_LIST_X, y + (box_h - ASSET_CURSOR_H) / 2 + 1, ASSET_CURSOR_W, ASSET_CURSOR_H,
                               ASSET_CURSOR_DATA, COL_BLACK);
        display_draw_string(box_x + 3, y + 3, label, COL_BLACK, COL_WHITE, SUB_LABEL_SCALE);
    } else {
        display_draw_string_fg(text_x, y + 3, label, COL_DIM, SUB_LABEL_SCALE);
    }
}

static void draw_sub_footer(const char *footer)
{
    display_draw_fill_rect(0, LCD_H - SUB_FOOTER_H, LCD_W, SUB_FOOTER_H, COL_WHITE);
    char row[48];
    int max_chars = (LCD_W - 12) / 8;
    int n = 0;
    if (footer) {
        while (footer[n] && n < max_chars && n < (int)sizeof(row) - 1) {
            row[n] = footer[n];
            n++;
        }
    }
    row[n] = 0;
    display_draw_string(8, LCD_H - SUB_FOOTER_H + 4, row, COL_BLACK, COL_WHITE, 1);
}

static void draw_bt_active_bar(bool hid_active, bool airspam_on)
{
    esp_lcd_panel_handle_t panel = lcd_panel_handle();
    const int y = LCD_H - SUB_FOOTER_H - SUB_ACTIVE_H;
    for (int row = 0; row < SUB_ACTIVE_H; row++) {
        int sy = y + row;
        if (sy < 0 || sy >= LCD_H) {
            continue;
        }
        uint16_t line[LCD_W];
        for (int x = 0; x < LCD_W; x++) {
            uint16_t p = ASSET_SUBMENU_BG_DATA[sy * ASSET_SUBMENU_BG_W + x];
            line[x] = (uint16_t)((p >> 8) | (p << 8));
        }
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, sy, LCD_W, sy + 1, line));
    }

    display_draw_string_fg(8, y + 3, "active:", COL_WHITE, 1);
    const char *active = bt_active_label(hid_active, airspam_on);
    uint16_t ac = (active[0] == 'n') ? COL_DIM : COL_GREEN;
    display_draw_string_fg(8 + display_draw_string_width("active:", 1) + 4, y + 3, active, ac, 1);
}

void menu_screens_draw_category_submenu(int cat_index, int selected, bool full,
                                        bool hid_active, bool airspam_on)
{
    const category_t *cat = &CATEGORIES[cat_index];
    const bool is_bt = (cat_index == MENU_MAIN_BT);

    if (full) {
        draw_submenu_background();
        display_draw_string_fg(8, 8, cat->header, COL_DIM, 1);
        for (int i = 0; i < cat->count; i++) {
            draw_sub_list_row(cat, i, i == selected);
        }
    } else {
        for (int i = 0; i < cat->count; i++) {
            draw_sub_list_row(cat, i, i == selected);
        }
    }

    const sub_item_t *item = &cat->items[selected];
    bool hid_busy = (item->action == ACT_HID_KBD) && hid_active;
    draw_sub_panel(item, hid_busy);
    if (is_bt) {
        draw_bt_active_bar(hid_active, airspam_on);
    }
    draw_sub_footer(item->footer);
}
