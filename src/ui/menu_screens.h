/**
 * Screen painters — no input handling.
 *
 * Main menu uses partial redraws (`full_bg == false`) so scroll only
 * restores the focus band. Submenu `active:` strip reflects live BLE state.
 */
#pragma once

#include <stdbool.h>

#define SPLASH_MS 1800

void menu_screens_draw_splash(void);
void menu_screens_splash_glitch_out(void);

/** @param full_bg true = tile background + focus; false = patch focus band only */
void menu_screens_draw_main_menu(int selected, bool full_bg);

void menu_screens_draw_category_submenu(int cat_index, int selected, bool full,
                                        bool hid_active, bool apple_spam_on);

/** Live heap / DRAM / flash / task stats. full=true redraws chrome. */
void menu_screens_draw_resources(bool full);

/**
 * Evil Bluetooth scanner list.
 * @param selected 0..n-1 = device, n = Back
 * @param scroll_top first visible device index
 */
void menu_screens_draw_evil_bt(int selected, int scroll_top, bool full);

typedef enum {
    MENU_LIST_WIFI_SCAN = 0,
    MENU_LIST_WIFI_PORTAL,
    MENU_LIST_WIFI_PORTAL_LOG,
} menu_list_kind_t;

void menu_screens_draw_list(menu_list_kind_t kind, int selected, int scroll, bool full);

#define MENU_PORTAL_LOG_VISIBLE 5
#define MENU_PORTAL_LOG_ROW_H   32

void menu_screens_portal_log_adjust_scroll(int sel, int count, int *scroll);
void menu_screens_draw_portal_log(int selected, int scroll, bool full);
