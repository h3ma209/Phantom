/**
 * UI paint layer — splash, main focus carousel, category submenu.
 *
 * Layout constants below are intentional “magic”; change them together.
 * Scroll path restores the focus band from the tiled menu background so we
 * never full-clear the screen on every detent.
 */
#include "menu_screens.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_ops.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_flash.h"
#include "esp_partition.h"
#include "display_draw.h"
#include "lcd_panel.h"
#include "menu_catalog.h"
#include "ble_hid_keyboard.h"
#include "ble_airspam.h"
#include "ble_clone.h"
#include "wifi_manager.h"
#include "wifi_portals.h"
#include "menu_list.h"
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

/* Tile low-opacity menu BG into a rect — used to erase focus without flicker. */
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

/* Single centered category. full_bg=false → restore band then redraw item. */
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
    } else if (item->action == ACT_FAKE_AP) {
        bool on = wifi_manager_ap_running();
        status = on ? item->status_busy : "idle";
        status_col = on ? COL_GREEN : COL_DIM;
        conn = on ? wifi_manager_ip() : "stopped";
        conn_col = on ? COL_GREEN : COL_DIM;
    } else if (item->action == ACT_CLONE_AP) {
        status = "idle";
        conn = "scan to pick";
        conn_col = COL_DIM;
    } else if (item->action == ACT_PORTAL) {
        status = wifi_portal_name(wifi_manager_portal());
        status_col = COL_PURPLE;
        conn = "template";
        conn_col = COL_DIM;
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

    const char *name = item->name_val;
    if (item->action == ACT_FAKE_AP || item->action == ACT_CLONE_AP) {
        name = wifi_manager_ssid();
    } else if (item->action == ACT_PORTAL) {
        name = wifi_portal_name(wifi_manager_portal());
    }
    draw_panel_field(tx, ty + row * 36, "name", name, COL_PURPLE);
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

/* Strip just above footer: which BT attack is armed (none / Keyboard / AirSpam). */
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

/* --- Resources monitor --- */

#define RES_BAR_X   88
#define RES_BAR_W   200
#define RES_BAR_H   10
#define RES_ROW0    36
#define RES_ROW_H   40

static void draw_usage_bar(int y, int pct)
{
    if (pct < 0) {
        pct = 0;
    }
    if (pct > 100) {
        pct = 100;
    }
    display_draw_rect(RES_BAR_X, y, RES_BAR_W, RES_BAR_H, COL_WHITE);
    int fill = (RES_BAR_W - 2) * pct / 100;
    uint16_t col = COL_GREEN;
    if (pct >= 85) {
        col = COL_RED;
    } else if (pct >= 65) {
        col = COL_PURPLE;
    }
    if (fill > 0) {
        display_draw_fill_rect(RES_BAR_X + 1, y + 1, fill, RES_BAR_H - 2, col);
    }
    if (fill < RES_BAR_W - 2) {
        display_draw_fill_rect(RES_BAR_X + 1 + fill, y + 1, (RES_BAR_W - 2) - fill, RES_BAR_H - 2, COL_BLACK);
    }
}

static void draw_res_row(int row, const char *name, int pct, const char *detail)
{
    int y = RES_ROW0 + row * RES_ROW_H;
    display_draw_fill_rect(8, y, LCD_W - 16, RES_ROW_H - 4, COL_BLACK);
    display_draw_string_fg(8, y, name, COL_WHITE, 1);
    char pct_s[8];
    snprintf(pct_s, sizeof(pct_s), "%3d%%", pct);
    display_draw_string_fg(RES_BAR_X + RES_BAR_W + 6, y, pct_s, COL_WHITE, 1);
    draw_usage_bar(y + 12, pct);
    display_draw_string6_fg(8, y + 26, detail, COL_DIM);
}

/* App image size via partition header walk — no bootloader_support link cost. */
static size_t app_image_len(const esp_partition_t *part)
{
    if (!part) {
        return 0;
    }
    uint8_t magic = 0;
    if (esp_partition_read(part, 0, &magic, 1) != ESP_OK || magic != 0xE9) {
        return 0;
    }
    uint8_t seg_count = 0;
    if (esp_partition_read(part, 1, &seg_count, 1) != ESP_OK || seg_count > 16) {
        return 0;
    }
    size_t off = 24; /* esp_image_header_t */
    size_t total = 24;
    for (uint8_t i = 0; i < seg_count; i++) {
        uint32_t len = 0;
        if (esp_partition_read(part, off + 4, &len, 4) != ESP_OK) {
            return 0;
        }
        if (len > part->size) {
            return 0;
        }
        off += 8 + len;
        total += 8 + len;
        /* segment data padded to 16 on flash */
        size_t pad = (16 - (len & 15)) & 15;
        off += pad;
        total += pad;
    }
    uint8_t hash_appended = 0;
    (void)esp_partition_read(part, 23, &hash_appended, 1);
    if (hash_appended == 1) {
        total += 32;
    }
    return total;
}

void menu_screens_draw_resources(bool full)
{
    if (full) {
        draw_submenu_background();
        display_draw_string_fg(8, 8, "resources", COL_DIM, 1);
        draw_sub_footer("click to go back");
    }

    size_t heap_free = esp_get_free_heap_size();
    size_t heap_min = esp_get_minimum_free_heap_size();
    size_t heap_total = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    size_t heap_used = (heap_total > heap_free) ? (heap_total - heap_free) : 0;
    int heap_pct = heap_total ? (int)((heap_used * 100) / heap_total) : 0;

    size_t dram_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t dram_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t dram_used = (dram_total > dram_free) ? (dram_total - dram_free) : 0;
    int dram_pct = dram_total ? (int)((dram_used * 100) / dram_total) : 0;

    const esp_partition_t *app = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    if (!app) {
        app = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    }
    size_t part_size = app ? app->size : 0;
    size_t img_len = app_image_len(app);
    int flash_pct = (part_size && img_len) ? (int)((img_len * 100) / part_size) : 0;

    uint32_t chip_flash = 0;
    (void)esp_flash_get_size(NULL, &chip_flash);

    UBaseType_t tasks = uxTaskGetNumberOfTasks();

    char detail[44];
    snprintf(detail, sizeof(detail), "%u/%u KB  minfree %u KB",
             (unsigned)(heap_used / 1024), (unsigned)(heap_total / 1024),
             (unsigned)(heap_min / 1024));
    draw_res_row(0, "HEAP", heap_pct, detail);

    snprintf(detail, sizeof(detail), "%u/%u KB internal",
             (unsigned)(dram_used / 1024), (unsigned)(dram_total / 1024));
    draw_res_row(1, "DRAM", dram_pct, detail);

    if (img_len && part_size) {
        snprintf(detail, sizeof(detail), "app %u/%u KB  chip %u MB",
                 (unsigned)(img_len / 1024), (unsigned)(part_size / 1024),
                 (unsigned)(chip_flash / (1024 * 1024)));
    } else {
        snprintf(detail, sizeof(detail), "slot %u KB  chip %u MB",
                 (unsigned)(part_size / 1024),
                 (unsigned)(chip_flash / (1024 * 1024)));
    }
    draw_res_row(2, "FLASH", flash_pct, detail);

    int y = RES_ROW0 + 3 * RES_ROW_H;
    display_draw_fill_rect(8, y, LCD_W - 16, RES_ROW_H - 4, COL_BLACK);
    display_draw_string_fg(8, y, "TASKS", COL_WHITE, 1);
    snprintf(detail, sizeof(detail), "%u FreeRTOS tasks", (unsigned)tasks);
    display_draw_string_fg(88, y, detail, COL_PURPLE, 1);
    display_draw_string6_fg(8, y + 14, "live sample — click back", COL_DIM);
}

/* --- Evil Bluetooth scanner --- */

#define EBT_ROW_H     22
#define EBT_LIST_Y0   28
#define EBT_VISIBLE   7
#define EBT_NAME_MAX  22

void menu_screens_draw_evil_bt(int selected, int scroll_top, bool full)
{
    const int n = ble_clone_device_count();
    const int back_idx = n; /* last row = Back */
    const int rows = n + 1;

    if (full) {
        draw_submenu_background();
        display_draw_string_fg(8, 8, "evil bluetooth", COL_DIM, 1);
    } else {
        display_draw_fill_rect(8, 8, LCD_W - 16, 14, COL_BLACK);
        display_draw_string_fg(8, 8, "evil bluetooth", COL_DIM, 1);
    }

    const char *st = ble_clone_is_active() ? "CLONING" :
                     (ble_clone_is_scanning() ? "SCAN" : "IDLE");
    uint16_t st_col = ble_clone_is_active() ? COL_GREEN :
                      (ble_clone_is_scanning() ? COL_PURPLE : COL_DIM);
    display_draw_string_fg(LCD_W - 8 - display_draw_string_width(st, 1), 8, st, st_col, 1);

    display_draw_fill_rect(6, EBT_LIST_Y0, LCD_W - 12, EBT_VISIBLE * EBT_ROW_H, COL_BLACK);

    for (int vis = 0; vis < EBT_VISIBLE; vis++) {
        int idx = scroll_top + vis;
        if (idx >= rows) {
            break;
        }
        int y = EBT_LIST_Y0 + vis * EBT_ROW_H;
        bool sel = (idx == selected);

        if (idx == back_idx) {
            if (sel) {
                display_draw_fill_rect(8, y + 1, 60, EBT_ROW_H - 4, COL_WHITE);
                display_draw_string(12, y + 4, "Back", COL_BLACK, COL_WHITE, 1);
            } else {
                display_draw_string_fg(12, y + 4, "Back", COL_DIM, 1);
            }
            continue;
        }

        ble_clone_dev_t dev;
        if (!ble_clone_device_get(idx, &dev)) {
            continue;
        }

        /* Device name as text — never MAC hex */
        char name[EBT_NAME_MAX + 1];
        const char *src = (dev.named && dev.name[0]) ? dev.name :
                          (dev.name[0] ? dev.name : "Unknown");
        int ni = 0;
        while (src[ni] && ni < EBT_NAME_MAX) {
            char c = src[ni];
            name[ni] = (c >= 32 && c <= 126) ? c : '?';
            ni++;
        }
        name[ni] = 0;

        char info[28];
        snprintf(info, sizeof(info), "%d dBm", (int)dev.rssi);

        if (sel) {
            display_draw_fill_rect(8, y + 1, LCD_W - 16, EBT_ROW_H - 4, COL_WHITE);
            display_draw_string(12, y + 3, name, COL_BLACK, COL_WHITE, 1);
            display_draw_string6(12, y + 12, info, COL_BLACK, COL_WHITE);
        } else {
            display_draw_string_fg(12, y + 3, name, COL_WHITE, 1);
            display_draw_string6_fg(12, y + 12, info, COL_DIM);
        }
    }

    const char *foot;
    if (ble_clone_is_active()) {
        foot = "cloning — click Back or device to stop";
    } else if (selected == back_idx) {
        foot = "click to leave";
    } else if (n == 0) {
        foot = "scanning for named BLE devices…";
    } else {
        foot = "click name to clone";
    }
    draw_sub_footer(foot);

    if (ble_clone_is_active()) {
        char bar[40];
        snprintf(bar, sizeof(bar), "clone: %s", ble_clone_active_name());
        display_draw_fill_rect(0, LCD_H - SUB_FOOTER_H - 14, LCD_W, 14, COL_BLACK);
        display_draw_string_fg(8, LCD_H - SUB_FOOTER_H - 11, bar, COL_GREEN, 1);
    }
}

static void wifi_scanning_row(int index, char *title, size_t title_n, char *sub, size_t sub_n, void *ctx)
{
    (void)ctx;
    if (index == 0) {
        strncpy(title, "Scanning...", title_n - 1);
        title[title_n - 1] = 0;
        snprintf(sub, sub_n, "please wait");
    } else {
        title[0] = 0;
        sub[0] = 0;
    }
}

static void wifi_scan_row(int index, char *title, size_t title_n, char *sub, size_t sub_n, void *ctx)
{
    (void)ctx;
    wifi_ap_info_t ap;
    if (!wifi_manager_ap_get(index, &ap)) {
        title[0] = 0;
        sub[0] = 0;
        return;
    }
    strncpy(title, ap.ssid, title_n - 1);
    title[title_n - 1] = 0;
    snprintf(sub, sub_n, "%d dBm  ch%u", (int)ap.rssi, (unsigned)ap.channel);
}

static void wifi_portal_row(int index, char *title, size_t title_n, char *sub, size_t sub_n, void *ctx)
{
    (void)ctx;
    const char *n = wifi_portal_name(index);
    if (index == wifi_manager_portal()) {
        snprintf(title, title_n, "* %s", n);
    } else {
        snprintf(title, title_n, "  %s", n);
    }
    sub[0] = 0;
    (void)sub_n;
}

void menu_screens_draw_list(menu_list_kind_t kind, int selected, int scroll, bool full)
{
    if (kind == MENU_LIST_WIFI_PORTAL) {
        const int count = WIFI_PORTAL_COUNT + 1;
        menu_list_draw("captive portal", wifi_portal_name(wifi_manager_portal()), COL_PURPLE,
                       count, selected, scroll, wifi_portal_row, NULL, full,
                       selected == WIFI_PORTAL_COUNT ? "click to leave" : "click to select template");
        return;
    }

    if (wifi_manager_scan_busy()) {
        menu_list_draw("clone ap", "SCAN", COL_PURPLE, 2, selected, scroll,
                       wifi_scanning_row, NULL, full, "scanning nearby APs…");
        return;
    }

    int n = wifi_manager_ap_count();
    int count = n + 1;
    const char *st = (n == 0) ? "SCAN" : "LIST";
    uint16_t sc = (n == 0) ? COL_PURPLE : COL_GREEN;
    const char *foot;
    if (selected == n) {
        foot = "click to leave";
    } else if (n == 0) {
        foot = "no APs — back and retry";
    } else {
        foot = "click SSID to clone name";
    }
    menu_list_draw("clone ap", st, sc, count, selected, scroll, wifi_scan_row, NULL, full, foot);
}
