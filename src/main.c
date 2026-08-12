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
    SCR_BT,
    SCR_HID,
    SCR_DETAIL,
} screen_t;

typedef struct {
    const char *title;
    const char *subtitle;
    const uint16_t *img;
    int img_w;
    int img_h;
} menu_item_t;

static const menu_item_t MENU_MAIN[] = {
    {"Recon", "Scan / map targets", IMG_ICON_RECON_DATA, IMG_ICON_RECON_W, IMG_ICON_RECON_H},
    {"Deauth Attacks", "Kick clients off AP", IMG_ICON_DEAUTH_DATA, IMG_ICON_DEAUTH_W, IMG_ICON_DEAUTH_H},
    {"Evil Twin", "Rogue AP tools", IMG_ICON_EVIL_DATA, IMG_ICON_EVIL_W, IMG_ICON_EVIL_H},
    {"Bluetooth Attacks", "BLE HID tools", IMG_ICON_BT_DATA, IMG_ICON_BT_W, IMG_ICON_BT_H},
    {"Remote Control", "Remote ops", IMG_ICON_REMOTE_DATA, IMG_ICON_REMOTE_W, IMG_ICON_REMOTE_H},
};
static const int MENU_MAIN_COUNT = sizeof(MENU_MAIN) / sizeof(MENU_MAIN[0]);

static const menu_item_t MENU_BT[] = {
    {"HID Keyboard", "Pair and type to PC", NULL, 0, 0},
};
static const int MENU_BT_COUNT = sizeof(MENU_BT) / sizeof(MENU_BT[0]);

#define MENU_MAIN_BT 3

static esp_lcd_panel_handle_t s_panel;

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
        /* horizontal slice tears */
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
                        line[x] = 0; /* black tear */
                    } else {
                        uint16_t p = src[sx];
                        line[x] = (uint16_t)((p >> 8) | (p << 8));
                    }
                }
                ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel, 0, y, LCD_W, y + 1, line));
            }
        }

        /* block wipe intensifies toward end */
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

/* Fixed focus slot so scroll only patches one band (no full-screen flash). */
#define FOCUS_ICON_SLOT_W 90
#define FOCUS_ICON_SLOT_H 100
#define FOCUS_GAP         14
#define FOCUS_TITLE_SCALE 2
#define FOCUS_TITLE_H     (8 * FOCUS_TITLE_SCALE)
#define FOCUS_BLOCK_H     (FOCUS_ICON_SLOT_H + FOCUS_GAP + FOCUS_TITLE_H)
#define FOCUS_Y0          ((LCD_H - FOCUS_BLOCK_H) / 2)

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

static void draw_bt_menu(int selected, bool full_bg)
{
    draw_focus_item(&MENU_BT[selected], full_bg);
}

static void draw_detail(const menu_item_t *item)
{
    draw_menu_background();

    if (item->img && item->img_w > 0) {
        int ix = (LCD_W - item->img_w) / 2;
        gfx_draw_image_key(ix, 28, item->img_w, item->img_h, item->img, COL_BLACK);
    }

    int tw = gfx_string_width(item->title, 2);
    if (tw > LCD_W - 8) {
        tw = gfx_string_width(item->title, 1);
        gfx_draw_string_fg((LCD_W - tw) / 2, 140, item->title, COL_WHITE, 1);
    } else {
        gfx_draw_string_fg((LCD_W - tw) / 2, 140, item->title, COL_WHITE, 2);
    }

    int sw = gfx_string_width(item->subtitle, 1);
    gfx_draw_string_fg((LCD_W - sw) / 2, 164, item->subtitle, COL_DIM, 1);

    const char *stub = "coming soon";
    int stub_w = gfx_string_width(stub, 1);
    gfx_draw_string_fg((LCD_W - stub_w) / 2, 190, stub, COL_GREEN, 1);
}

static void draw_hid(bool typed)
{
    draw_menu_background();
    gfx_draw_icon_scaled((LCD_W - 72) / 2, 28, ICON_CHIP, COL_WHITE, 3);

    const char *title = "HID Keyboard";
    int tw = gfx_string_width(title, 2);
    gfx_draw_string_fg((LCD_W - tw) / 2, 112, title, COL_WHITE, 2);
    gfx_draw_string_fg(24, 140, "Name: DEDSEC KBD", COL_DIM, 1);

    if (hid_kbd_is_connected()) {
        gfx_draw_string_fg(24, 158, "Status: CONNECTED", COL_GREEN, 1);
        gfx_draw_string_fg(24, 174, "CLICK types demo text", COL_WHITE, 1);
        if (typed) {
            gfx_draw_string_fg(24, 190, "Sent.", COL_GREEN, 1);
        }
    } else {
        gfx_draw_string_fg(24, 158, "Status: advertising", COL_GREEN, 1);
        gfx_draw_string_fg(24, 174, "Pair DEDSEC KBD on PC", COL_WHITE, 1);
    }
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
    int sel_bt = 0;
    int detail_index = 0;
    bool hid_typed = false;
    bool hid_was_conn = false;

    draw_main_menu(sel_main, true);
    ESP_LOGI(TAG, "Menu ready (encoder P25/P26/P27)");

    while (1) {
        if (screen == SCR_HID) {
            bool conn = hid_kbd_is_connected();
            if (conn != hid_was_conn) {
                hid_was_conn = conn;
                if (conn) {
                    vTaskDelay(pdMS_TO_TICKS(400));
                    hid_kbd_type(HID_DEMO_TEXT);
                    hid_typed = true;
                }
                draw_hid(hid_typed);
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
                if (sel_main == MENU_MAIN_BT) {
                    screen = SCR_BT;
                    sel_bt = 0;
                    draw_bt_menu(sel_bt, true);
                } else {
                    screen = SCR_DETAIL;
                    detail_index = sel_main;
                    draw_detail(&MENU_MAIN[detail_index]);
                }
            }
        } else if (screen == SCR_BT) {
            if (ev == ENC_CW) {
                sel_bt = (sel_bt + 1) % MENU_BT_COUNT;
                draw_bt_menu(sel_bt, false);
            } else if (ev == ENC_CCW) {
                screen = SCR_MAIN;
                draw_main_menu(sel_main, true);
            } else if (ev == ENC_CLICK) {
                screen = SCR_HID;
                hid_typed = false;
                hid_was_conn = false;
                draw_hid(false);
                if (hid_kbd_start() != ESP_OK) {
                    gfx_draw_string_fg(24, 200, "BT start failed", COL_RED, 1);
                } else {
                    draw_hid(false);
                }
            }
        } else if (screen == SCR_HID) {
            if (ev == ENC_CCW || ev == ENC_CW) {
                screen = SCR_BT;
                draw_bt_menu(sel_bt, true);
            } else if (ev == ENC_CLICK) {
                if (hid_kbd_is_connected()) {
                    hid_kbd_type(HID_DEMO_TEXT);
                    hid_typed = true;
                }
                draw_hid(hid_typed);
            }
        } else if (screen == SCR_DETAIL) {
            if (ev == ENC_CLICK) {
                screen = SCR_MAIN;
                draw_main_menu(sel_main, true);
            }
        }
    }
}
