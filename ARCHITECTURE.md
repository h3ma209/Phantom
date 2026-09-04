# Phantom — Architecture

ESP32-S NodeMCU + ILI9341 TFT (320×240 landscape). PlatformIO / ESP-IDF.

WiFi behavior matches **pengz0** (scan → copy SSID → Fake AP + DNS + captive HTML). Code layout does **not** copy pengz0’s Arduino menus.

**Educational project only** — authorized lab use. See [README.md](README.md#disclaimer) for full disclaimer.

## Boot path

`app_main` → LCD + encoder → splash → glitch-out → `menu_nav_run()` (blocks forever).

## Layout (`src/`)

| Dir | Role |
|-----|------|
| `board/` | Pin map (`board_pins.h`) |
| `display/` | Panel bring-up, blit helpers, RGB565 assets |
| `input/` | Rotary encoder ISR → queue |
| `ui/` | Catalog, screens, nav, shared `menu_list` |
| `wifi/` | **wifi_manager** owns radio; scan / AP / DNS / portals are thin |
| `features/` | BLE HID, Apple Spam, BLE clone |

## WiFi manager

UI never calls `esp_wifi_*`. Modes: `WIFI_MGR_OFF` / `WIFI_MGR_SCAN` / `WIFI_MGR_AP` (`wifi_mgr_mode_t` — IDF already owns `wifi_mode_t`). Switching stops the previous mode.

- **Scan** — STA scan into a fixed cache (SSID, RSSI, channel).
- **Clone SSID** — copy scanned name into Fake AP SSID (not BSSID/channel).
- **Fake AP** — open soft AP at `172.217.28.1`, UDP DNS hijack (`*` → AP), HTTP portal on captive probe paths.
- **Portals** — four HTML templates (IQ / Komar / MyKomar / Komar Cap).
- **Tick** — `wifi_manager_tick()` in the nav loop while AP is up (DNS).

BLE is paused when WiFi scan or AP starts.

## UI model

- **Main**: centered category carousel.
- **Submenu**: left list + right panel. Evil Twin: Fake AP toggle, Clone AP list, Captive Portal list.
- **Shared list** (`menu_list`): AP picker and portal picker (Back last row).

Encoder: turn = move; click = activate. Fake AP click toggles on/off.

## BLE sharing

One NimBLE host. HID / Apple Spam / clone mutually pause advertising. Do not `ble_gap_adv_stop()` before host sync.

## Hardware gotchas

- TFT **DC on GPIO2** — flaky upload. See `WIRING.md`.
- App partition ~3MB (`partitions.csv`).

## Assets

PNG → RGB565 headers under `display/assets/`. Source art in `images/`.
