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
                                        bool hid_active, bool airspam_on);

/** Live heap / DRAM / flash / task stats. full=true redraws chrome. */
void menu_screens_draw_resources(bool full);

/**
 * Evil Bluetooth scanner list.
 * @param selected 0..n-1 = device, n = Back
 * @param scroll_top first visible device index
 */
void menu_screens_draw_evil_bt(int selected, int scroll_top, bool full);
