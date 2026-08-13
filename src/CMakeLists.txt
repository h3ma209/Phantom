idf_component_register(
  SRCS
    "main.c"
    "display/lcd_panel.c"
    "display/display_draw.c"
    "input/rotary_encoder.c"
    "ui/menu_catalog.c"
    "ui/menu_screens.c"
    "ui/menu_nav.c"
    "features/ble_hid_keyboard.c"
    "features/ble_airspam.c"
    "features/ble_hid_gap.c"
  INCLUDE_DIRS
    "."
    "board"
    "display"
    "display/assets"
    "input"
    "ui"
    "features"
  REQUIRES esp_hid esp_lcd driver
  PRIV_REQUIRES nvs_flash bt
)
