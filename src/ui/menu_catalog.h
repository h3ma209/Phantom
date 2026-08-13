#pragma once

#include <stdint.h>

typedef enum {
    ACT_NONE = 0,
    ACT_BACK,
    ACT_HID_KBD,
    ACT_AIRSPAM,
    ACT_SOON,
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

#define MENU_MAIN_BT 3

extern const menu_item_t MENU_MAIN[];
extern const int MENU_MAIN_COUNT;
extern const category_t CATEGORIES[];
