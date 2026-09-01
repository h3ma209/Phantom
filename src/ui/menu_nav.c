/**
 * Menu state machine + feature arming.
 *
 * HID/AirSpam stay armed across Back so you can leave the submenu while
 * advertising. Only ACT_BACK returns to main; attack toggles stay local.
 */
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
#include "ble_clone.h"

static const char *TAG = "menu_nav";

#define HID_DEMO_TEXT "hello from dedsec node\n"
#define RES_REFRESH_MS 500
#define EBT_REFRESH_MS 400
#define EBT_VISIBLE 7

typedef enum {
    SCR_MAIN = 0,
    SCR_SUB,
    SCR_RES,
    SCR_EVIL_BT,
} screen_t;

static void ebt_pause_other_ble(bool *hid_active, bool *airspam_on)
{
    if (*airspam_on) {
        ble_airspam_stop();
        *airspam_on = false;
    }
    if (*hid_active) {
        *hid_active = false;
        ble_hid_keyboard_set_want_adv(false);
        ble_hid_keyboard_pause_adv();
    }
}

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
    TickType_t res_last = 0;
    TickType_t ebt_last = 0;
    int ebt_sel = 0;
    int ebt_scroll = 0;
    uint32_t ebt_gen = 0;

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

        if (screen == SCR_RES) {
            TickType_t now = xTaskGetTickCount();
            if ((now - res_last) >= pdMS_TO_TICKS(RES_REFRESH_MS)) {
                res_last = now;
                menu_screens_draw_resources(false);
            }
        }

        if (screen == SCR_EVIL_BT) {
            TickType_t now = xTaskGetTickCount();
            uint32_t gen = ble_clone_list_gen();
            if (gen != ebt_gen || (now - ebt_last) >= pdMS_TO_TICKS(EBT_REFRESH_MS)) {
                ebt_gen = gen;
                ebt_last = now;
                /* Keep selection in range as list grows */
                int n = ble_clone_device_count();
                int max_sel = n; /* Back */
                if (ebt_sel > max_sel) {
                    ebt_sel = max_sel;
                }
                if (ebt_sel < ebt_scroll) {
                    ebt_scroll = ebt_sel;
                }
                if (ebt_sel >= ebt_scroll + EBT_VISIBLE) {
                    ebt_scroll = ebt_sel - EBT_VISIBLE + 1;
                }
                menu_screens_draw_evil_bt(ebt_sel, ebt_scroll, false);
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
                if (sel_main == MENU_MAIN_RESOURCES) {
                    screen = SCR_RES;
                    res_last = xTaskGetTickCount();
                    menu_screens_draw_resources(true);
                } else if (sel_main == MENU_MAIN_EVIL_BT) {
                    ebt_pause_other_ble(&hid_active, &airspam_on);
                    screen = SCR_EVIL_BT;
                    ebt_sel = 0;
                    ebt_scroll = 0;
                    ebt_gen = 0;
                    ebt_last = xTaskGetTickCount();
                    if (ble_clone_start_scan() != ESP_OK) {
                        ESP_LOGW(TAG, "evil bt scan failed");
                    }
                    menu_screens_draw_evil_bt(ebt_sel, ebt_scroll, true);
                } else {
                    cat_index = sel_main;
                    sel_sub = 0;
                    screen = SCR_SUB;
                    menu_screens_draw_category_submenu(cat_index, sel_sub, true, hid_active, airspam_on);
                }
            }
        } else if (screen == SCR_RES) {
            if (ev == ROTENC_CLICK) {
                screen = SCR_MAIN;
                menu_screens_draw_main_menu(sel_main, true);
            }
        } else if (screen == SCR_EVIL_BT) {
            int n = ble_clone_device_count();
            int max_sel = n; /* Back at end */
            if (ev == ROTENC_CW) {
                ebt_sel = (ebt_sel + 1) % (max_sel + 1);
            } else if (ev == ROTENC_CCW) {
                ebt_sel = (ebt_sel - 1 + max_sel + 1) % (max_sel + 1);
            } else if (ev == ROTENC_CLICK) {
                if (ebt_sel == max_sel) {
                    /* Back */
                    ble_clone_stop();
                    ble_clone_stop_scan();
                    screen = SCR_MAIN;
                    menu_screens_draw_main_menu(sel_main, true);
                    continue;
                }
                if (ble_clone_is_active()) {
                    ble_clone_stop();
                    /* Resume scanning after stop */
                    (void)ble_clone_start_scan();
                } else {
                    if (ble_clone_start(ebt_sel) != ESP_OK) {
                        ESP_LOGW(TAG, "clone failed idx=%d", ebt_sel);
                        (void)ble_clone_start_scan();
                    }
                }
            }
            if (ebt_sel < ebt_scroll) {
                ebt_scroll = ebt_sel;
            }
            if (ebt_sel >= ebt_scroll + EBT_VISIBLE) {
                ebt_scroll = ebt_sel - EBT_VISIBLE + 1;
            }
            menu_screens_draw_evil_bt(ebt_sel, ebt_scroll, false);
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
                    screen = SCR_MAIN;
                    menu_screens_draw_main_menu(sel_main, true);
                } else if (item->action == ACT_HID_KBD) {
                    if (airspam_on) {
                        ble_airspam_stop();
                        airspam_on = false;
                    }
                    ble_clone_stop();
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
                    ble_clone_stop();
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
