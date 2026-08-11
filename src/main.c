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
#include "image_data.h"
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
#define SPLASH_MS 2500

#define MENU_VISIBLE 6
#define MENU_ROW_H   28
#define MENU_Y0      34

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
    ui_icon_t icon;
} menu_item_t;

static const menu_item_t MENU_MAIN[] = {
    {"Status", "Node health / uptime", ICON_CHIP},
    {"Network", "Scan nearby APs", ICON_WIFI},
    {"Radar", "Signal map", ICON_RADAR},
    {"Ops Log", "Recent events", ICON_LIST},
    {"BT Exploits", "BLE HID tools", ICON_SHIELD},
    {"Settings", "Display / system", ICON_GEAR},
    {"About", "DEDSEC node info", ICON_INFO},
    {"Power", "Sleep / restart", ICON_POWER},
};
static const int MENU_MAIN_COUNT = sizeof(MENU_MAIN) / sizeof(MENU_MAIN[0]);

static const menu_item_t MENU_BT[] = {
    {"HID Keyboard", "Pair and type to PC", ICON_CHIP},
};
static const int MENU_BT_COUNT = sizeof(MENU_BT) / sizeof(MENU_BT[0]);

#define MENU_MAIN_BT 4

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

static int menu_scroll_for(int selected)
{
    if (selected >= MENU_VISIBLE) {
        return selected - MENU_VISIBLE + 1;
    }
    return 0;
}

static void draw_menu_row(const menu_item_t *items, int item, int slot, bool selected)
{
    const int y = MENU_Y0 + slot * MENU_ROW_H;
    uint16_t bg = selected ? COL_SELECT : COL_BLACK;
    uint16_t fg = selected ? COL_GREEN : COL_WHITE;

    gfx_fill_rect(0, y, LCD_W, MENU_ROW_H, bg);
    gfx_draw_icon(4, y + 2, items[item].icon, fg, bg);
    gfx_draw_string(34, y + 10, items[item].title, fg, bg, 1);
    if (selected) {
        gfx_draw_string(LCD_W - 16, y + 10, ">", fg, bg, 1);
    }
}

static void draw_splash(void)
{
    gfx_fill_screen(COL_BLACK);
    const int x0 = (LCD_W - IMG_W) / 2;
    const int y0 = (LCD_H - IMG_H) / 2;
    gfx_draw_image_rgb565(x0, y0, IMG_W, IMG_H, IMG_DATA);
}

static void draw_list(const char *subtitle, const menu_item_t *items, int count, int selected)
{
    const int scroll = menu_scroll_for(selected);

    gfx_fill_screen(COL_BLACK);
    gfx_fill_rect(0, 0, LCD_W, 28, COL_PANEL);
    gfx_draw_string(8, 6, "DEDSEC", COL_GREEN, COL_PANEL, 2);
    gfx_draw_string(120, 10, subtitle, COL_WHITE, COL_PANEL, 1);

    for (int n = 0; n < MENU_VISIBLE; n++) {
        int i = scroll + n;
        if (i >= count) {
            break;
        }
        draw_menu_row(items, i, n, i == selected);
    }

    gfx_fill_rect(0, LCD_H - 18, LCD_W, 18, COL_PANEL);
    gfx_draw_string(8, LCD_H - 14, "TURN: move   CLICK: open", COL_DIM, COL_PANEL, 1);
}

static void menu_set_selected(const menu_item_t *items, int count, int *selected, int next)
{
    int prev = *selected;
    if (next == prev) {
        return;
    }
    int scroll_prev = menu_scroll_for(prev);
    int scroll_next = menu_scroll_for(next);
    *selected = next;

    if (scroll_prev != scroll_next) {
        draw_list(items == MENU_BT ? "BT EXPLOITS" : "MAIN MENU", items, count, next);
        return;
    }

    draw_menu_row(items, prev, prev - scroll_next, false);
    draw_menu_row(items, next, next - scroll_next, true);
}

static void draw_detail(const menu_item_t *item)
{
    gfx_fill_screen(COL_BLACK);
    gfx_fill_rect(0, 0, LCD_W, 28, COL_PANEL);
    gfx_draw_string(8, 6, "DEDSEC", COL_GREEN, COL_PANEL, 2);

    gfx_draw_icon(16, 50, item->icon, COL_GREEN, COL_BLACK);
    gfx_draw_string(50, 54, item->title, COL_WHITE, COL_BLACK, 2);
    gfx_draw_string(50, 78, item->subtitle, COL_DIM, COL_BLACK, 1);

    gfx_draw_string(16, 120, "Module stub — coming soon.", COL_GREEN, COL_BLACK, 1);
    gfx_draw_string(16, 140, "Wire logic later.", COL_DIM, COL_BLACK, 1);

    gfx_fill_rect(0, LCD_H - 18, LCD_W, 18, COL_PANEL);
    gfx_draw_string(8, LCD_H - 14, "CLICK: back to menu", COL_DIM, COL_PANEL, 1);
}

static void draw_hid(bool typed)
{
    gfx_fill_screen(COL_BLACK);
    gfx_fill_rect(0, 0, LCD_W, 28, COL_PANEL);
    gfx_draw_string(8, 6, "DEDSEC", COL_GREEN, COL_PANEL, 2);
    gfx_draw_string(120, 10, "HID KBD", COL_WHITE, COL_PANEL, 1);

    gfx_draw_icon(16, 48, ICON_CHIP, COL_GREEN, COL_BLACK);
    gfx_draw_string(50, 52, "HID Keyboard", COL_WHITE, COL_BLACK, 2);
    gfx_draw_string(16, 88, "Name: DEDSEC KBD", COL_DIM, COL_BLACK, 1);

    if (hid_kbd_is_connected()) {
        gfx_draw_string(16, 110, "Status: CONNECTED", COL_GREEN, COL_BLACK, 1);
        gfx_draw_string(16, 128, "CLICK types demo text", COL_WHITE, COL_BLACK, 1);
        if (typed) {
            gfx_draw_string(16, 146, "Sent.", COL_GREEN, COL_BLACK, 1);
        }
    } else {
        gfx_draw_string(16, 110, "Status: advertising", COL_GREEN, COL_BLACK, 1);
        gfx_draw_string(16, 128, "PC: add Bluetooth device", COL_WHITE, COL_BLACK, 1);
        gfx_draw_string(16, 146, "Pair DEDSEC KBD", COL_DIM, COL_BLACK, 1);
    }

    gfx_fill_rect(0, LCD_H - 18, LCD_W, 18, COL_PANEL);
    gfx_draw_string(8, LCD_H - 14, "CLICK: type/refresh  turn: back", COL_DIM, COL_PANEL, 1);
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

    screen_t screen = SCR_MAIN;
    int sel_main = 0;
    int sel_bt = 0;
    int detail_index = 0;
    bool hid_typed = false;
    bool hid_was_conn = false;

    draw_list("MAIN MENU", MENU_MAIN, MENU_MAIN_COUNT, sel_main);
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
                menu_set_selected(MENU_MAIN, MENU_MAIN_COUNT, &sel_main,
                                  (sel_main + 1) % MENU_MAIN_COUNT);
            } else if (ev == ENC_CCW) {
                menu_set_selected(MENU_MAIN, MENU_MAIN_COUNT, &sel_main,
                                  (sel_main - 1 + MENU_MAIN_COUNT) % MENU_MAIN_COUNT);
            } else if (ev == ENC_CLICK) {
                if (sel_main == MENU_MAIN_BT) {
                    screen = SCR_BT;
                    sel_bt = 0;
                    draw_list("BT EXPLOITS", MENU_BT, MENU_BT_COUNT, sel_bt);
                } else {
                    screen = SCR_DETAIL;
                    detail_index = sel_main;
                    draw_detail(&MENU_MAIN[detail_index]);
                }
            }
        } else if (screen == SCR_BT) {
            if (ev == ENC_CW) {
                menu_set_selected(MENU_BT, MENU_BT_COUNT, &sel_bt,
                                  (sel_bt + 1) % MENU_BT_COUNT);
            } else if (ev == ENC_CCW) {
                screen = SCR_MAIN;
                draw_list("MAIN MENU", MENU_MAIN, MENU_MAIN_COUNT, sel_main);
            } else if (ev == ENC_CLICK) {
                screen = SCR_HID;
                hid_typed = false;
                hid_was_conn = false;
                draw_hid(false);
                if (hid_kbd_start() != ESP_OK) {
                    gfx_draw_string(16, 164, "BT start failed", COL_RED, COL_BLACK, 1);
                } else {
                    draw_hid(false);
                }
            }
        } else if (screen == SCR_HID) {
            if (ev == ENC_CCW || ev == ENC_CW) {
                screen = SCR_BT;
                draw_list("BT EXPLOITS", MENU_BT, MENU_BT_COUNT, sel_bt);
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
                draw_list("MAIN MENU", MENU_MAIN, MENU_MAIN_COUNT, sel_main);
            }
        }
    }
}
