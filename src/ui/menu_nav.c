#include "menu_nav.h"

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "rotary_encoder.h"
#include "menu_catalog.h"
#include "menu_screens.h"
#include "ble_hid_keyboard.h"
#include "ble_airspam.h"

static const char *TAG = "menu_nav";

#define HID_DEMO_TEXT "hello from dedsec node\n"

typedef enum {
    SCR_MAIN = 0,
    SCR_SUB,
} screen_t;

void menu_nav_run(void)
{
    screen_t screen = SCR_MAIN;
    int sel_main = MENU_MAIN_BT;
    int sel_sub = 0;
    int cat_index = MENU_MAIN_BT;
    bool hid_was_conn = false;
    bool hid_active = false;
    bool hid_started = false;
    bool airspam_on = false;

    menu_screens_draw_main_menu(sel_main, true);
    ESP_LOGI(TAG, "Menu ready (encoder P25/P26/P27)");

    while (1) {
        if (screen == SCR_SUB && cat_index == MENU_MAIN_BT && hid_active) {
            bool conn = ble_hid_keyboard_is_connected();
            if (conn != hid_was_conn) {
                hid_was_conn = conn;
                if (conn) {
                    vTaskDelay(pdMS_TO_TICKS(400));
                    ble_hid_keyboard_type(HID_DEMO_TEXT);
                }
                menu_screens_draw_category_submenu(cat_index, sel_sub, false, hid_active, airspam_on);
            }
        }

        rotenc_event_t ev = rotary_encoder_poll();
        if (ev == ROTENC_NONE) {
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        if (screen == SCR_MAIN) {
            if (ev == ROTENC_CW) {
                sel_main = (sel_main + 1) % MENU_MAIN_COUNT;
                menu_screens_draw_main_menu(sel_main, false);
            } else if (ev == ROTENC_CCW) {
                sel_main = (sel_main - 1 + MENU_MAIN_COUNT) % MENU_MAIN_COUNT;
                menu_screens_draw_main_menu(sel_main, false);
            } else if (ev == ROTENC_CLICK) {
                cat_index = sel_main;
                sel_sub = 0;
                screen = SCR_SUB;
                menu_screens_draw_category_submenu(cat_index, sel_sub, true, hid_active, airspam_on);
            }
        } else if (screen == SCR_SUB) {
            const category_t *cat = &CATEGORIES[cat_index];
            if (ev == ROTENC_CW) {
                sel_sub = (sel_sub + 1) % cat->count;
                menu_screens_draw_category_submenu(cat_index, sel_sub, false, hid_active, airspam_on);
            } else if (ev == ROTENC_CCW) {
                sel_sub = (sel_sub - 1 + cat->count) % cat->count;
                menu_screens_draw_category_submenu(cat_index, sel_sub, false, hid_active, airspam_on);
            } else if (ev == ROTENC_CLICK) {
                const sub_item_t *item = &cat->items[sel_sub];
                if (item->action == ACT_BACK) {
                    /* leave running attacks; only Back exits submenu */
                    screen = SCR_MAIN;
                    menu_screens_draw_main_menu(sel_main, true);
                } else if (item->action == ACT_HID_KBD) {
                    if (airspam_on) {
                        ble_airspam_stop();
                        airspam_on = false;
                    }
                    if (!hid_started) {
                        if (ble_hid_keyboard_start() == ESP_OK) {
                            hid_started = true;
                            hid_active = true;
                            ble_hid_keyboard_set_want_adv(true);
                            hid_was_conn = false;
                        }
                    } else {
                        hid_active = !hid_active;
                        ble_hid_keyboard_set_want_adv(hid_active);
                        if (hid_active) {
                            ble_hid_keyboard_resume_adv_if_active();
                            if (ble_hid_keyboard_is_connected()) {
                                ble_hid_keyboard_type(HID_DEMO_TEXT);
                            }
                        } else {
                            ble_hid_keyboard_pause_adv();
                        }
                    }
                    menu_screens_draw_category_submenu(cat_index, sel_sub, false, hid_active, airspam_on);
                } else if (item->action == ACT_AIRSPAM) {
                    if (hid_active) {
                        hid_active = false;
                        ble_hid_keyboard_set_want_adv(false);
                        ble_hid_keyboard_pause_adv();
                    }
                    if (!airspam_on) {
                        if (ble_airspam_start() == ESP_OK) {
                            airspam_on = true;
                        }
                    } else {
                        ble_airspam_stop();
                        airspam_on = false;
                    }
                    menu_screens_draw_category_submenu(cat_index, sel_sub, false, hid_active, airspam_on);
                } else {
                    menu_screens_draw_category_submenu(cat_index, sel_sub, false, hid_active, airspam_on);
                }
            }
        }
    }
}
