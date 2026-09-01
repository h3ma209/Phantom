#include "menu_list.h"

#include <stdio.h>
#include <string.h>
#include "display_draw.h"
#include "lcd_panel.h"
#include "esp_lcd_panel_ops.h"
#include "asset_submenu_bg.h"

#define LIST_FOOTER_H 16
#define TITLE_MAX 22
#define SUB_MAX 28

static void draw_submenu_bg(void)
{
    display_draw_image_rgb565(0, 0, ASSET_SUBMENU_BG_W, ASSET_SUBMENU_BG_H, ASSET_SUBMENU_BG_DATA);
}

static void draw_footer(const char *footer)
{
    display_draw_fill_rect(0, LCD_H - LIST_FOOTER_H, LCD_W, LIST_FOOTER_H, COL_WHITE);
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
    display_draw_string(8, LCD_H - LIST_FOOTER_H + 4, row, COL_BLACK, COL_WHITE, 1);
}

void menu_list_adjust_scroll(int sel, int count, int *scroll)
{
    if (!scroll) {
        return;
    }
    if (count < 1) {
        *scroll = 0;
        return;
    }
    if (sel < *scroll) {
        *scroll = sel;
    }
    if (sel >= *scroll + MENU_LIST_VISIBLE) {
        *scroll = sel - MENU_LIST_VISIBLE + 1;
    }
    if (*scroll < 0) {
        *scroll = 0;
    }
}

void menu_list_draw(const char *header, const char *status, uint16_t status_col,
                    int count, int selected, int scroll,
                    menu_list_row_fn row_fn, void *ctx,
                    bool full, const char *footer)
{
    if (full) {
        draw_submenu_bg();
    } else {
        display_draw_fill_rect(8, 8, LCD_W - 16, 14, COL_BLACK);
    }

    display_draw_string_fg(8, 8, header ? header : "", COL_DIM, 1);
    if (status && status[0]) {
        int sw = display_draw_string_width(status, 1);
        display_draw_string_fg(LCD_W - 8 - sw, 8, status, status_col, 1);
    }

    display_draw_fill_rect(6, MENU_LIST_Y0, LCD_W - 12, MENU_LIST_VISIBLE * MENU_LIST_ROW_H, COL_BLACK);

    const int back_idx = count > 0 ? count - 1 : 0;

    for (int vis = 0; vis < MENU_LIST_VISIBLE; vis++) {
        int idx = scroll + vis;
        if (idx >= count) {
            break;
        }
        int y = MENU_LIST_Y0 + vis * MENU_LIST_ROW_H;
        bool sel = (idx == selected);

        if (idx == back_idx) {
            if (sel) {
                display_draw_fill_rect(8, y + 1, 60, MENU_LIST_ROW_H - 4, COL_WHITE);
                display_draw_string(12, y + 4, "Back", COL_BLACK, COL_WHITE, 1);
            } else {
                display_draw_string_fg(12, y + 4, "Back", COL_DIM, 1);
            }
            continue;
        }

        char title[TITLE_MAX + 1];
        char sub[SUB_MAX + 1];
        title[0] = 0;
        sub[0] = 0;
        if (row_fn) {
            row_fn(idx, title, sizeof(title), sub, sizeof(sub), ctx);
        }

        if (sel) {
            display_draw_fill_rect(8, y + 1, LCD_W - 16, MENU_LIST_ROW_H - 4, COL_WHITE);
            display_draw_string(12, y + 3, title, COL_BLACK, COL_WHITE, 1);
            if (sub[0]) {
                display_draw_string6(12, y + 12, sub, COL_BLACK, COL_WHITE);
            }
        } else {
            display_draw_string_fg(12, y + 3, title, COL_WHITE, 1);
            if (sub[0]) {
                display_draw_string6_fg(12, y + 12, sub, COL_DIM);
            }
        }
    }

    draw_footer(footer);
}
