/**
 * Evil Bluetooth — scan nearby BLE advertisers, clone selected ADV identity.
 *
 * Clone = re-advertise captured AD bytes (name / mfg data / etc). MAC is our
 * random static address (can't steal public identity without controller tricks).
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define BLE_CLONE_MAX_DEVS  16
#define BLE_CLONE_NAME_LEN  24
#define BLE_CLONE_ADV_MAX   31

typedef struct {
    uint8_t addr[6];       /* host order (NimBLE) */
    uint8_t addr_type;
    char name[BLE_CLONE_NAME_LEN];
    int8_t rssi;
    uint8_t adv[BLE_CLONE_ADV_MAX];
    uint8_t adv_len;
    bool named;
} ble_clone_dev_t;

esp_err_t ble_clone_ensure_stack(void);

/** Start continuous discovery (active scan for names). */
esp_err_t ble_clone_start_scan(void);
void ble_clone_stop_scan(void);
bool ble_clone_is_scanning(void);

int ble_clone_device_count(void);
/** Copy device snapshot; false if index OOB. */
bool ble_clone_device_get(int index, ble_clone_dev_t *out);

/** Stop scan and advertise as device[index]. Click again via stop. */
esp_err_t ble_clone_start(int index);
void ble_clone_stop(void);
bool ble_clone_is_active(void);
const char *ble_clone_active_name(void);

/** Bump when list changes — UI can skip redraw if unchanged. */
uint32_t ble_clone_list_gen(void);
