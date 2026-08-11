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

typedef struct {
    const char *title;
    const char *subtitle;
    ui_icon_t icon;
} menu_item_t;

static const menu_item_t MENU[] = {
    {"Status", "Node health / uptime", ICON_CHIP},
    {"Network", "Scan nearby APs", ICON_WIFI},
    {"Radar", "Signal map", ICON_RADAR},
    {"Ops Log", "Recent events", ICON_LIST},
    {"Shield", "Security posture", ICON_SHIELD},
    {"Settings", "Display / system", ICON_GEAR},
    {"About", "DEDSEC node info", ICON_INFO},
    {"Power", "Sleep / restart", ICON_POWER},
};
static const int MENU_COUNT = sizeof(MENU) / sizeof(MENU[0]);

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

static void draw_menu_row(int item, int slot, bool selected)
{
    const int y = MENU_Y0 + slot * MENU_ROW_H;
    uint16_t bg = selected ? COL_SELECT : COL_BLACK;
    uint16_t fg = selected ? COL_GREEN : COL_WHITE;

    gfx_fill_rect(0, y, LCD_W, MENU_ROW_H, bg);
    gfx_draw_icon(4, y + 2, MENU[item].icon, fg, bg);
    gfx_draw_string(34, y + 10, MENU[item].title, fg, bg, 1);
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

static void draw_menu(int selected)
{
    const int scroll = menu_scroll_for(selected);

    gfx_fill_screen(COL_BLACK);
    gfx_fill_rect(0, 0, LCD_W, 28, COL_PANEL);
    gfx_draw_string(8, 6, "DEDSEC", COL_GREEN, COL_PANEL, 2);
    gfx_draw_string(120, 10, "MAIN MENU", COL_WHITE, COL_PANEL, 1);

    for (int n = 0; n < MENU_VISIBLE; n++) {
        int i = scroll + n;
        if (i >= MENU_COUNT) {
            break;
        }
        draw_menu_row(i, n, i == selected);
    }

    gfx_fill_rect(0, LCD_H - 18, LCD_W, 18, COL_PANEL);
    gfx_draw_string(8, LCD_H - 14, "TURN: move   CLICK: open", COL_DIM, COL_PANEL, 1);
}

/* Only redraw 2 rows when scroll unchanged — big speed win. */
static void menu_set_selected(int *selected, int next)
{
    int prev = *selected;
    if (next == prev) {
        return;
    }
    int scroll_prev = menu_scroll_for(prev);
    int scroll_next = menu_scroll_for(next);
    *selected = next;

    if (scroll_prev != scroll_next) {
        draw_menu(next);
        return;
    }

    draw_menu_row(prev, prev - scroll_next, false);
    draw_menu_row(next, next - scroll_next, true);
}

static void draw_detail(int index)
{
    gfx_fill_screen(COL_BLACK);
    gfx_fill_rect(0, 0, LCD_W, 28, COL_PANEL);
    gfx_draw_string(8, 6, "DEDSEC", COL_GREEN, COL_PANEL, 2);

    gfx_draw_icon(16, 50, MENU[index].icon, COL_GREEN, COL_BLACK);
    gfx_draw_string(50, 54, MENU[index].title, COL_WHITE, COL_BLACK, 2);
    gfx_draw_string(50, 78, MENU[index].subtitle, COL_DIM, COL_BLACK, 1);

    gfx_draw_string(16, 120, "Module stub — coming soon.", COL_GREEN, COL_BLACK, 1);
    gfx_draw_string(16, 140, "Wire logic later.", COL_DIM, COL_BLACK, 1);

    gfx_fill_rect(0, LCD_H - 18, LCD_W, 18, COL_PANEL);
    gfx_draw_string(8, LCD_H - 14, "CLICK: back to menu", COL_DIM, COL_PANEL, 1);
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

    int selected = 0;
    bool in_detail = false;
    draw_menu(selected);
    ESP_LOGI(TAG, "Menu ready (encoder P25/P26/P27)");

    while (1) {
        enc_event_t ev = encoder_poll();
        if (ev == ENC_NONE) {
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        if (in_detail) {
            if (ev == ENC_CLICK) {
                in_detail = false;
                draw_menu(selected);
            }
            continue;
        }

        if (ev == ENC_CW) {
            menu_set_selected(&selected, (selected + 1) % MENU_COUNT);
        } else if (ev == ENC_CCW) {
            menu_set_selected(&selected, (selected - 1 + MENU_COUNT) % MENU_COUNT);
        } else if (ev == ENC_CLICK) {
            in_detail = true;
            draw_detail(selected);
        }
    }
}
