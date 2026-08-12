#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "img_startup.h"
#include "img_menu_bg.h"
#include "img_submenu_bg.h"
#include "img_cursor.h"
#include "img_icon_recon.h"
#include "img_icon_deauth.h"
#include "img_icon_evil.h"
#include "img_icon_bt.h"
#include "img_icon_remote.h"
#include "gfx.h"
#include "encoder.h"

#include "hid_kbd.h"

static const char *TAG = "dedsec";

#define PIN_MOSI 23
#define PIN_SCLK 18
#define PIN_CS   15
#define PIN_DC   2
#define PIN_RST  4
#define PIN_BL   21

#define SPI_CLOCK_HZ (40 * 1000 * 1000)
#define LCD_HOST SPI2_HOST
#define SPLASH_MS 1800
#define GLITCH_FRAMES 10
#define GLITCH_FRAME_MS 110

#define HID_DEMO_TEXT "hello from dedsec node\n"

typedef enum {
    SCR_MAIN = 0,
    SCR_SUB,
} screen_t;

typedef enum {
    ACT_NONE = 0,
    ACT_BACK,
    ACT_HID_KBD,
    ACT_SOON,
} sub_action_t;

typedef struct {
    const char *title;
    const char *subtitle;
    const uint16_t *img;
    int img_w;
    int img_h;
} menu_item_t;

typedef struct {
    const char *label;
    const char *footer;
    const char *name_val;
    const char *status_idle;
    const char *status_busy;
    const char *conn_val;
    const char *hint;
    sub_action_t action;
} sub_item_t;

typedef struct {
    const char *header;
    const sub_item_t *items;
    int count;
} category_t;

static const menu_item_t MENU_MAIN[] = {
    {"Recon", "Scan / map targets", IMG_ICON_RECON_DATA, IMG_ICON_RECON_W, IMG_ICON_RECON_H},
    {"Deauth Attacks", "Kick clients off AP", IMG_ICON_DEAUTH_DATA, IMG_ICON_DEAUTH_W, IMG_ICON_DEAUTH_H},
    {"Evil Twin", "Rogue AP tools", IMG_ICON_EVIL_DATA, IMG_ICON_EVIL_W, IMG_ICON_EVIL_H},
    {"Bluetooth Attacks", "BLE HID tools", IMG_ICON_BT_DATA, IMG_ICON_BT_W, IMG_ICON_BT_H},
    {"Remote Control", "Remote ops", IMG_ICON_REMOTE_DATA, IMG_ICON_REMOTE_W, IMG_ICON_REMOTE_H},
};
static const int MENU_MAIN_COUNT = sizeof(MENU_MAIN) / sizeof(MENU_MAIN[0]);

static const sub_item_t SUB_RECON[] = {
    {"Probe Scan", "Passive probe request sniff", "—", "idle", "scanning", "n/a", "coming soon", ACT_SOON},
    {"AP Map", "Map nearby access points", "—", "idle", "running", "n/a", "coming soon", ACT_SOON},
    {"Back", "Return to main menu", "—", "—", "—", "—", "click to go back", ACT_BACK},
};

static const sub_item_t SUB_DEAUTH[] = {
    {"Deauth Burst", "Kick clients with deauth frames", "target", "idle", "working", "not armed", "coming soon", ACT_SOON},
    {"Deauth Flood", "Continuous deauth flood", "target", "idle", "flooding", "not armed", "coming soon", ACT_SOON},
    {"Back", "Return to main menu", "—", "—", "—", "—", "click to go back", ACT_BACK},
};

static const sub_item_t SUB_EVIL[] = {
    {"Clone AP", "Stand up a twin of target AP", "twin", "idle", "spoofing", "down", "coming soon", ACT_SOON},
    {"Captive Portal", "Phish via captive portal", "portal", "idle", "live", "down", "coming soon", ACT_SOON},
    {"Back", "Return to main menu", "—", "—", "—", "—", "click to go back", ACT_BACK},
};

static const sub_item_t SUB_BT[] = {
    {"As Keyboard", "Act as BT keyboard — go rogue", "Lenovo Keyboard", "idle", "Working",
     "not connected", "press again to stop", ACT_HID_KBD},
    {"Airpod Spam", "Spam Apple BLE popups", "AirPods", "idle", "spamming", "n/a", "coming soon", ACT_SOON},
    {"Jammer", "BLE jammer — soon", "—", "soon", "soon", "n/a", "not ready", ACT_SOON},
    {"Back", "Return to main menu", "—", "—", "—", "—", "click to go back", ACT_BACK},
};

static const sub_item_t SUB_REMOTE[] = {
    {"HID Replay", "Replay captured HID input", "device", "idle", "replaying", "n/a", "coming soon", ACT_SOON},
    {"Back", "Return to main menu", "—", "—", "—", "—", "click to go back", ACT_BACK},
};

static const category_t CATEGORIES[] = {
    {"recon", SUB_RECON, (int)(sizeof(SUB_RECON) / sizeof(SUB_RECON[0]))},
    {"deauth attacks", SUB_DEAUTH, (int)(sizeof(SUB_DEAUTH) / sizeof(SUB_DEAUTH[0]))},
    {"evil twin", SUB_EVIL, (int)(sizeof(SUB_EVIL) / sizeof(SUB_EVIL[0]))},
    {"bluetooth attacks", SUB_BT, (int)(sizeof(SUB_BT) / sizeof(SUB_BT[0]))},
    {"remote control", SUB_REMOTE, (int)(sizeof(SUB_REMOTE) / sizeof(SUB_REMOTE[0]))},
};

#define MENU_MAIN_BT 3

/* Fixed focus slot so scroll only patches one band (no full-screen flash). */
#define FOCUS_ICON_SLOT_W 90
#define FOCUS_ICON_SLOT_H 100
#define FOCUS_GAP         14
#define FOCUS_TITLE_SCALE 2
#define FOCUS_TITLE_H     (8 * FOCUS_TITLE_SCALE)
#define FOCUS_BLOCK_H     (FOCUS_ICON_SLOT_H + FOCUS_GAP + FOCUS_TITLE_H)
#define FOCUS_Y0          ((LCD_H - FOCUS_BLOCK_H) / 2)

#define SUB_LIST_X     16
#define SUB_LIST_Y0    28
#define SUB_ROW_H      16
#define SUB_CURSOR_GAP 3
#define SUB_PANEL_X    170
#define SUB_PANEL_Y    24
#define SUB_PANEL_W    140
#define SUB_PANEL_H    178
#define SUB_FOOTER_H   14
#define SUB_LABEL_MAX  16 /* chars at 8px before clip */

static esp_lcd_panel_handle_t s_panel;
static bool s_hid_active;
static bool s_hid_started;

static void backlight_on(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << PIN_BL,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    gpio_set_level(PIN_BL, 1);
}

static uint16_t prng_next(uint16_t *state)
{
    *state = (uint16_t)(*state * 1103515245u + 12345u);
    return *state;
}

static void draw_splash(void)
{
    gfx_draw_image_rgb565(0, 0, IMG_STARTUP_W, IMG_STARTUP_H, IMG_STARTUP_DATA);
}

static void splash_glitch_out(void)
{
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
                const uint16_t *src = &IMG_STARTUP_DATA[y * IMG_STARTUP_W];
                for (int x = 0; x < LCD_W; x++) {
                    int sx = x - shift;
                    if (sx < 0 || sx >= LCD_W || (prng_next(&rng) & 0x3F) == 0) {
                        line[x] = 0;
                    } else {
                        uint16_t p = src[sx];
                        line[x] = (uint16_t)((p >> 8) | (p << 8));
                    }
                }
                ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel, 0, y, LCD_W, y + 1, line));
            }
        }

        int blocks = 2 + frame;
        for (int b = 0; b < blocks; b++) {
            int bx = prng_next(&rng) % LCD_W;
            int by = prng_next(&rng) % LCD_H;
            int bw = 8 + (prng_next(&rng) % 48);
            int bh = 4 + (prng_next(&rng) % 24);
            gfx_fill_rect(bx, by, bw, bh, COL_BLACK);
        }

        vTaskDelay(pdMS_TO_TICKS(GLITCH_FRAME_MS));
    }

    gfx_fill_screen(COL_BLACK);
}

static void blit_menu_bg_rect(int x0, int y0, int w, int h)
{
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
        int ty = y % IMG_MENU_BG_H;
        for (int x = x0; x < x0 + w; x++) {
            int tx = x % IMG_MENU_BG_W;
            uint16_t p = IMG_MENU_BG_DATA[ty * IMG_MENU_BG_W + tx];
            line[x - x0] = (uint16_t)((p >> 8) | (p << 8));
        }
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel, x0, y, x0 + w, y + 1, line));
    }
}

static void draw_menu_background(void)
{
    blit_menu_bg_rect(0, 0, LCD_W, LCD_H);
}

static void draw_submenu_background(void)
{
    gfx_draw_image_rgb565(0, 0, IMG_SUBMENU_BG_W, IMG_SUBMENU_BG_H, IMG_SUBMENU_BG_DATA);
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
        gfx_draw_image_key(ix, iy, item->img_w, item->img_h, item->img, COL_BLACK);
    } else {
        gfx_draw_icon_scaled(ix, iy, ICON_CHIP, COL_WHITE, 3);
    }

    int title_scale = FOCUS_TITLE_SCALE;
    int tw = gfx_string_width(item->title, title_scale);
    if (tw > LCD_W - 8) {
        title_scale = 1;
        tw = gfx_string_width(item->title, title_scale);
    }
    int tx = (LCD_W - tw) / 2;
    if (tx < 0) {
        tx = 0;
    }
    gfx_draw_string_fg(tx, FOCUS_Y0 + FOCUS_ICON_SLOT_H + FOCUS_GAP, item->title, COL_WHITE, title_scale);
}

static void draw_main_menu(int selected, bool full_bg)
{
    draw_focus_item(&MENU_MAIN[selected], full_bg);
}

static void draw_panel_field(int x, int y, const char *key, const char *val, uint16_t val_color)
{
    /* label secondary (6px dim), value primary (8px color) stacked */
    gfx_draw_string6_fg(x, y, key, COL_DIM);
    int max_chars = (SUB_PANEL_W - 20) / 8;
    char truncated[24];
    int n = 0;
    if (val) {
        while (val[n] && n < max_chars && n < (int)sizeof(truncated) - 1) {
            truncated[n] = val[n];
            n++;
        }
    }
    truncated[n] = 0;
    gfx_draw_string_fg(x, y + 8, truncated, val_color, 1);
}

static void draw_sub_panel(const sub_item_t *item, bool hid_busy)
{
    gfx_fill_rect(SUB_PANEL_X + 3, SUB_PANEL_Y + 3, SUB_PANEL_W - 6, SUB_PANEL_H - 6, COL_BLACK);
    gfx_draw_rect_sketch(SUB_PANEL_X, SUB_PANEL_Y, SUB_PANEL_W, SUB_PANEL_H, COL_WHITE);

    const char *status = item->status_idle;
    uint16_t status_col = COL_DIM;
    const char *conn = item->conn_val;
    uint16_t conn_col = COL_RED;

    if (item->action == ACT_HID_KBD) {
        status = hid_busy ? item->status_busy : "idle";
        status_col = hid_busy ? COL_GREEN : COL_DIM;
        if (hid_kbd_is_connected()) {
            conn = "connected";
            conn_col = COL_GREEN;
        } else {
            conn = "not connected";
            conn_col = COL_RED;
        }
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

    int tx = SUB_PANEL_X + 10;
    int ty = SUB_PANEL_Y + 12;
    draw_panel_field(tx, ty, "name", item->name_val, COL_PURPLE);
    draw_panel_field(tx, ty + 32, "status", status, status_col);
    draw_panel_field(tx, ty + 64, "connection", conn, conn_col);

    if (item->hint && item->action != ACT_BACK) {
        int hx = SUB_PANEL_X + 8;
        int hy = SUB_PANEL_Y + SUB_PANEL_H - 28;
        /* muted hint, wrap at ~20 chars of 6px */
        const char *p = item->hint;
        int line = 0;
        char row[22];
        while (*p && line < 2) {
            int n = 0;
            while (*p && n < 20) {
                row[n++] = *p++;
            }
            row[n] = 0;
            gfx_draw_string6_fg(hx, hy + line * 8, row, COL_DIM);
            line++;
        }
    }
}

static void draw_sub_list_row(const category_t *cat, int index, bool selected)
{
    const sub_item_t *item = &cat->items[index];
    const int y = SUB_LIST_Y0 + index * SUB_ROW_H;
    const int text_x = SUB_LIST_X + IMG_CURSOR_W + SUB_CURSOR_GAP;
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
            uint16_t p = IMG_SUBMENU_BG_DATA[sy * IMG_SUBMENU_BG_W + (x0 + x)];
            line[x] = (uint16_t)((p >> 8) | (p << 8));
        }
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel, x0, sy, x0 + w, sy + 1, line));
    }

    char label[24];
    int n = 0;
    while (item->label[n] && n < SUB_LABEL_MAX && n < (int)sizeof(label) - 1) {
        label[n] = item->label[n];
        n++;
    }
    label[n] = 0;

    if (selected) {
        int label_w = gfx_string_width(label, 1);
        if (label_w > max_w) {
            label_w = max_w;
        }
        int box_w = label_w + 4;
        int box_x = text_x - 2;
        gfx_fill_rect(box_x, y + 2, box_w, 12, COL_WHITE);
        gfx_draw_image_key(SUB_LIST_X, y + 2, IMG_CURSOR_W, IMG_CURSOR_H, IMG_CURSOR_DATA, COL_BLACK);
        gfx_draw_string(box_x + 2, y + 4, label, COL_BLACK, COL_WHITE, 1);
    } else {
        gfx_draw_string_fg(text_x, y + 4, label, COL_DIM, 1);
    }
}

static void draw_sub_footer(const char *footer)
{
    gfx_fill_rect(0, LCD_H - SUB_FOOTER_H, LCD_W, SUB_FOOTER_H, COL_WHITE);
    char row[48];
    int max_chars = (LCD_W - 12) / 6;
    int n = 0;
    if (footer) {
        while (footer[n] && n < max_chars && n < (int)sizeof(row) - 1) {
            row[n] = footer[n];
            n++;
        }
    }
    row[n] = 0;
    gfx_draw_string6(8, LCD_H - SUB_FOOTER_H + 4, row, COL_BLACK, COL_WHITE);
}

static void draw_category_submenu(int cat_index, int selected, bool full)
{
    const category_t *cat = &CATEGORIES[cat_index];
    if (full) {
        draw_submenu_background();
        gfx_draw_string6_fg(8, 8, cat->header, COL_DIM);
        for (int i = 0; i < cat->count; i++) {
            draw_sub_list_row(cat, i, i == selected);
        }
    } else {
        for (int i = 0; i < cat->count; i++) {
            draw_sub_list_row(cat, i, i == selected);
        }
    }

    const sub_item_t *item = &cat->items[selected];
    bool hid_busy = (item->action == ACT_HID_KBD) && s_hid_active;
    draw_sub_panel(item, hid_busy);
    draw_sub_footer(item->footer);
}

static void lcd_init(void)
{
    backlight_on();

    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_W * 40 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_CS,
        .dc_gpio_num = PIN_DC,
        .spi_mode = 0,
        .pclk_hz = SPI_CLOCK_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io, &panel_config, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    gfx_init(s_panel);
    gfx_fill_screen(COL_BLACK);
}

void app_main(void)
{
    lcd_init();
    encoder_init();

    ESP_LOGI(TAG, "Splash");
    draw_splash();
    vTaskDelay(pdMS_TO_TICKS(SPLASH_MS));
    splash_glitch_out();

    screen_t screen = SCR_MAIN;
    int sel_main = MENU_MAIN_BT;
    int sel_sub = 0;
    int cat_index = MENU_MAIN_BT;
    bool hid_was_conn = false;

    draw_main_menu(sel_main, true);
    ESP_LOGI(TAG, "Menu ready (encoder P25/P26/P27)");

    while (1) {
        if (screen == SCR_SUB && cat_index == MENU_MAIN_BT && s_hid_active) {
            bool conn = hid_kbd_is_connected();
            if (conn != hid_was_conn) {
                hid_was_conn = conn;
                if (conn) {
                    vTaskDelay(pdMS_TO_TICKS(400));
                    hid_kbd_type(HID_DEMO_TEXT);
                }
                draw_category_submenu(cat_index, sel_sub, false);
            }
        }

        enc_event_t ev = encoder_poll();
        if (ev == ENC_NONE) {
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        if (screen == SCR_MAIN) {
            if (ev == ENC_CW) {
                sel_main = (sel_main + 1) % MENU_MAIN_COUNT;
                draw_main_menu(sel_main, false);
            } else if (ev == ENC_CCW) {
                sel_main = (sel_main - 1 + MENU_MAIN_COUNT) % MENU_MAIN_COUNT;
                draw_main_menu(sel_main, false);
            } else if (ev == ENC_CLICK) {
                cat_index = sel_main;
                sel_sub = 0;
                screen = SCR_SUB;
                draw_category_submenu(cat_index, sel_sub, true);
            }
        } else if (screen == SCR_SUB) {
            const category_t *cat = &CATEGORIES[cat_index];
            if (ev == ENC_CW) {
                sel_sub = (sel_sub + 1) % cat->count;
                draw_category_submenu(cat_index, sel_sub, false);
            } else if (ev == ENC_CCW) {
                sel_sub = (sel_sub - 1 + cat->count) % cat->count;
                draw_category_submenu(cat_index, sel_sub, false);
            } else if (ev == ENC_CLICK) {
                const sub_item_t *item = &cat->items[sel_sub];
                if (item->action == ACT_BACK) {
                    s_hid_active = false;
                    screen = SCR_MAIN;
                    draw_main_menu(sel_main, true);
                } else if (item->action == ACT_HID_KBD) {
                    if (!s_hid_started) {
                        if (hid_kbd_start() == ESP_OK) {
                            s_hid_started = true;
                            s_hid_active = true;
                            hid_was_conn = false;
                        }
                    } else {
                        s_hid_active = !s_hid_active;
                        if (s_hid_active && hid_kbd_is_connected()) {
                            hid_kbd_type(HID_DEMO_TEXT);
                        }
                    }
                    draw_category_submenu(cat_index, sel_sub, false);
                } else {
                    /* soon / stub — refresh panel only */
                    draw_category_submenu(cat_index, sel_sub, false);
                }
            }
        }
    }
}
