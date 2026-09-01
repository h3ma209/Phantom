/**
 * Generic scrollable list — Clone AP, portal picker, later BLE/Recon.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define MENU_LIST_VISIBLE 7
#define MENU_LIST_ROW_H   22
#define MENU_LIST_Y0      28

/** count includes the Back row at the end (index == count-1). */
void menu_list_adjust_scroll(int sel, int count, int *scroll);

typedef void (*menu_list_row_fn)(int index, char *title, size_t title_n,
                                 char *sub, size_t sub_n, void *ctx);

void menu_list_draw(const char *header, const char *status, uint16_t status_col,
                    int count, int selected, int scroll,
                    menu_list_row_fn row_fn, void *ctx,
                    bool full, const char *footer);
