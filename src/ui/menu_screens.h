#pragma once

#include <stdbool.h>

#define SPLASH_MS 1800

void menu_screens_draw_splash(void);
void menu_screens_splash_glitch_out(void);
void menu_screens_draw_main_menu(int selected, bool full_bg);
void menu_screens_draw_category_submenu(int cat_index, int selected, bool full,
                                        bool hid_active, bool airspam_on);
