/**
 * Top-level UI state machine: main ↔ category submenu + feature dispatch.
 * Blocks forever after splash (called from app_main).
 */
#pragma once

void menu_nav_run(void);
