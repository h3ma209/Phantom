/**
 * Static menu data only — titles, panel copy, and action tags.
 * Navigation/dispatch lives in menu_nav; painting in menu_screens.
 */
#pragma once

#include <stdint.h>

typedef enum {
    ACT_NONE = 0,
    ACT_BACK,      /* leave submenu; attacks may keep running */
    ACT_HID_KBD,   /* BLE HID keyboard advertise / type demo */
    ACT_AIRSPAM,   /* Apple Continuity spam (toggle) */
    ACT_FAKE_AP,   /* WiFi soft AP + captive DNS/HTTP */
    ACT_CLONE_AP,  /* scan list → copy SSID */
    ACT_PORTAL,    /* pick captive HTML template */
    ACT_SOON,      /* stub — UI only */
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

/** Index of "Evil Twin" in MENU_MAIN / CATEGORIES. */
#define MENU_MAIN_EVIL 2
/** Index of "Bluetooth Attacks" in MENU_MAIN / CATEGORIES. */
#define MENU_MAIN_BT 3
/** Index of "Evil Bluetooth" — scan/clone screen, not CATEGORIES[]. */
#define MENU_MAIN_EVIL_BT 5
/** Index of "Resources" — dedicated live-stats screen, not CATEGORIES[]. */
#define MENU_MAIN_RESOURCES 6

extern const menu_item_t MENU_MAIN[];
extern const int MENU_MAIN_COUNT;
extern const category_t CATEGORIES[];
extern const int MENU_CATEGORY_COUNT;
