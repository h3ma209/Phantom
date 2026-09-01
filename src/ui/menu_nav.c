/**
 * Menu state machine + feature arming.
 *
 * HID/AirSpam stay armed across Back. WiFi goes through wifi_manager only.
 */
#include "menu_nav.h"

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "rotary_encoder.h"
#include "menu_catalog.h"
#include "menu_screens.h"
#include "menu_list.h"
#include "ble_hid_keyboard.h"
#include "ble_airspam.h"
#include "ble_clone.h"
#include "wifi_manager.h"
#include "wifi_portals.h"

static const char *TAG = "menu_nav";

#define HID_DEMO_TEXT "hello from dedsec node\n"
#define RES_REFRESH_MS 500
#define EBT_REFRESH_MS 400
#define EBT_VISIBLE 7
#define CLICK_GUARD_US 300000

static int64_t s_click_block_until;

static void block_clicks(void)
{
    s_click_block_until = esp_timer_get_time() + CLICK_GUARD_US;
}

static bool click_blocked(void)
{
    return esp_timer_get_time() < s_click_block_until;
}

typedef enum {
    SCR_MAIN = 0,
    SCR_SUB,
    SCR_RES,
    SCR_EVIL_BT,
    SCR_LIST,
} screen_t;

/** Submenu Back is last row — do not wrap CCW from first item onto Back. */
static int submenu_next(int sel, int count, rotenc_event_t ev)
{
    if (count < 2) {
        return sel;
    }
    const int back = count - 1;
    if (ev == ROTENC_CW) {
        return (sel < back) ? (sel + 1) : back;
    }
    if (ev == ROTENC_CCW) {
        return (sel > 0) ? (sel - 1) : 0;
    }
    return sel;
}

static void pause_ble(bool *hid_active, bool *airspam_on)
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
    if (ble_hid_keyboard_stack_ready()) {
        ble_clone_stop();
        ble_clone_stop_scan();
    } else {
        ble_clone_force_idle();
    }
}

void menu_nav_run(void)
{
    screen_t screen = SCR_MAIN;
    int sel_main = MENU_MAIN_EVIL;
    int sel_sub = 0;
    int cat_index = MENU_MAIN_EVIL;
    bool hid_was_conn = false;
    bool hid_active = false;
    bool hid_started = false;
    bool airspam_on = false;
    TickType_t res_last = 0;
    TickType_t ebt_last = 0;
    int ebt_sel = 0;
    int ebt_scroll = 0;
    uint32_t ebt_gen = 0;
    menu_list_kind_t list_kind = MENU_LIST_WIFI_SCAN;
    int list_sel = 0;
    int list_scroll = 0;
    bool list_scan_was_busy = false;

    menu_screens_draw_main_menu(sel_main, true);
    ESP_LOGI(TAG, "Menu ready (encoder P25/P26/P27)");

    while (1) {
        wifi_manager_tick();

        if (screen == SCR_LIST && list_kind == MENU_LIST_WIFI_SCAN) {
            bool busy = wifi_manager_scan_busy();
            if (list_scan_was_busy && !busy) {
                menu_screens_draw_list(list_kind, list_sel, list_scroll, false);
            }
            list_scan_was_busy = busy;
        }

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
                int n = ble_clone_device_count();
                int max_sel = n;
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

        if (ev == ROTENC_CLICK && click_blocked()) {
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
                    pause_ble(&hid_active, &airspam_on);
                    screen = SCR_EVIL_BT;
                    ebt_sel = 0;
                    ebt_scroll = 0;
                    ebt_gen = 0;
                    ebt_last = xTaskGetTickCount();
                    if (ble_clone_start_scan() != ESP_OK) {
                        ESP_LOGW(TAG, "evil bt scan failed");
                    }
                    menu_screens_draw_evil_bt(ebt_sel, ebt_scroll, true);
                } else if (sel_main >= MENU_CATEGORY_COUNT) {
                    ESP_LOGW(TAG, "no submenu for main idx %d", sel_main);
                } else {
                    cat_index = sel_main;
                    sel_sub = 0;
                    screen = SCR_SUB;
                    rotary_encoder_flush();
                    block_clicks();
                    menu_screens_draw_category_submenu(cat_index, sel_sub, true, hid_active, airspam_on);
                }
            }
        } else if (screen == SCR_RES) {
            if (ev == ROTENC_CLICK) {
                screen = SCR_MAIN;
                menu_screens_draw_main_menu(sel_main, true);
            }
        } else if (screen == SCR_LIST) {
            int nitems;
            if (list_kind == MENU_LIST_WIFI_PORTAL) {
                nitems = WIFI_PORTAL_COUNT + 1;
            } else if (wifi_manager_scan_busy()) {
                nitems = 2;
            } else {
                nitems = wifi_manager_ap_count() + 1;
            }
            if (ev == ROTENC_CW) {
                list_sel = (list_sel + 1) % nitems;
            } else if (ev == ROTENC_CCW) {
                list_sel = (list_sel - 1 + nitems) % nitems;
            } else if (ev == ROTENC_CLICK) {
                if (wifi_manager_scan_busy() && list_kind == MENU_LIST_WIFI_SCAN) {
                    continue;
                }
                int back = nitems - 1;
                if (list_sel == back) {
                    if (list_kind == MENU_LIST_WIFI_SCAN) {
                        (void)wifi_manager_set_mode(WIFI_MGR_OFF);
                    }
                    screen = SCR_SUB;
                    menu_screens_draw_category_submenu(cat_index, sel_sub, true, hid_active, airspam_on);
                    continue;
                }
                if (list_kind == MENU_LIST_WIFI_SCAN) {
                    if (wifi_manager_clone_ssid(list_sel) == ESP_OK) {
                        (void)wifi_manager_set_mode(WIFI_MGR_OFF);
                    }
                    screen = SCR_SUB;
                    menu_screens_draw_category_submenu(cat_index, sel_sub, true, hid_active, airspam_on);
                    continue;
                }
                wifi_manager_set_portal(list_sel);
                screen = SCR_SUB;
                menu_screens_draw_category_submenu(cat_index, sel_sub, true, hid_active, airspam_on);
                continue;
            }
            menu_list_adjust_scroll(list_sel, nitems, &list_scroll);
            menu_screens_draw_list(list_kind, list_sel, list_scroll, false);
        } else if (screen == SCR_EVIL_BT) {
            int n = ble_clone_device_count();
            int max_sel = n;
            if (ev == ROTENC_CW) {
                ebt_sel = (ebt_sel + 1) % (max_sel + 1);
            } else if (ev == ROTENC_CCW) {
                ebt_sel = (ebt_sel - 1 + max_sel + 1) % (max_sel + 1);
            } else if (ev == ROTENC_CLICK) {
                if (ebt_sel == max_sel) {
                    ble_clone_stop();
                    ble_clone_stop_scan();
                    screen = SCR_MAIN;
                    menu_screens_draw_main_menu(sel_main, true);
                    continue;
                }
                if (ble_clone_is_active()) {
                    ble_clone_stop();
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
            if (ev == ROTENC_CW || ev == ROTENC_CCW) {
                sel_sub = submenu_next(sel_sub, cat->count, ev);
                menu_screens_draw_category_submenu(cat_index, sel_sub, false, hid_active, airspam_on);
            } else if (ev == ROTENC_CLICK) {
                const sub_item_t *item = &cat->items[sel_sub];
                ESP_LOGI(TAG, "sub click idx=%d action=%d label=%s", sel_sub, (int)item->action, item->label);
                if (item->action == ACT_BACK) {
                    screen = SCR_MAIN;
                    block_clicks();
                    menu_screens_draw_main_menu(sel_main, true);
                } else if (item->action == ACT_FAKE_AP) {
                    pause_ble(&hid_active, &airspam_on);
                    if (wifi_manager_ap_running()) {
                        (void)wifi_manager_set_mode(WIFI_MGR_OFF);
                    } else {
                        if (wifi_manager_set_mode(WIFI_MGR_AP) != ESP_OK) {
                            ESP_LOGW(TAG, "fake AP start failed");
                        }
                    }
                    menu_screens_draw_category_submenu(cat_index, sel_sub, false, hid_active, airspam_on);
                } else if (item->action == ACT_CLONE_AP) {
                    pause_ble(&hid_active, &airspam_on);
                    list_kind = MENU_LIST_WIFI_SCAN;
                    list_sel = 0;
                    list_scroll = 0;
                    list_scan_was_busy = true;
                    screen = SCR_LIST;
                    rotary_encoder_flush();
                    block_clicks();
                    menu_screens_draw_list(list_kind, list_sel, list_scroll, true);
                    if (wifi_manager_begin_scan() != ESP_OK) {
                        ESP_LOGW(TAG, "wifi scan failed");
                        list_scan_was_busy = false;
                    }
                    continue;
                } else if (item->action == ACT_PORTAL) {
                    list_kind = MENU_LIST_WIFI_PORTAL;
                    list_sel = wifi_manager_portal();
                    list_scroll = 0;
                    screen = SCR_LIST;
                    rotary_encoder_flush();
                    block_clicks();
                    menu_list_adjust_scroll(list_sel, WIFI_PORTAL_COUNT + 1, &list_scroll);
                    menu_screens_draw_list(list_kind, list_sel, list_scroll, true);
                    continue;
                } else if (item->action == ACT_HID_KBD) {
                    if (airspam_on) {
                        ble_airspam_stop();
                        airspam_on = false;
                    }
                    ble_clone_stop();
                    (void)wifi_manager_set_mode(WIFI_MGR_OFF);
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
                    (void)wifi_manager_set_mode(WIFI_MGR_OFF);
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
