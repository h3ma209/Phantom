# Firmware modules — see ../ARCHITECTURE.md
idf_component_register(
  SRCS
    "main.c"
    "display/lcd_panel.c"
    "display/display_draw.c"
    "display/asset_zlib.c"
    "input/rotary_encoder.c"
    "ui/menu_catalog.c"
    "ui/menu_screens.c"
    "ui/menu_nav.c"
    "ui/menu_list.c"
    "wifi/wifi_manager.c"
    "wifi/wifi_scan.c"
    "wifi/wifi_ap.c"
    "wifi/wifi_dns.c"
    "wifi/wifi_portals.c"
    "wifi/wifi_portal_log.c"
    "wifi/wifi_portal_iq_logo.c"
    "features/ble_hid_keyboard.c"
    "features/ble_apple_spam.c"
    "features/ble_clone.c"
    "features/ble_hid_gap.c"
  INCLUDE_DIRS
    "."
    "board"
    "display"
    "display/assets"
    "input"
    "ui"
    "features"
    "wifi"
  REQUIRES esp_hid esp_lcd driver
  PRIV_REQUIRES nvs_flash bt spi_flash esp_wifi esp_http_server esp_netif lwip esp_event esp_rom
)
