idf_component_register(
  SRCS "main.c" "gfx.c" "encoder.c" "hid_kbd.c" "esp_hid_gap.c"
  INCLUDE_DIRS "."
  REQUIRES esp_hid esp_lcd driver
  PRIV_REQUIRES nvs_flash bt
)
