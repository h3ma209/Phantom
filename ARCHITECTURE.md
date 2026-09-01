# Architecture

ESP32-S NodeMCU + ILI9341 TFT (320×240 landscape). PlatformIO / ESP-IDF.

## Boot path

`app_main` → LCD + encoder → splash → glitch-out → `menu_nav_run()` (blocks forever).

## Layout (`src/`)

| Dir | Role |
|-----|------|
| `board/` | Pin map (`board_pins.h`) |
| `display/` | Panel bring-up, blit helpers, RGB565 assets |
| `input/` | Rotary encoder ISR → queue |
| `ui/` | Catalog data, screen paint, nav state machine |
| `features/` | BLE HID keyboard + Apple Continuity “AirSpam” |

## UI model

- **Main**: one centered category (icon + title). Scroll patches focus band only — avoids full-screen flicker.
- **Submenu**: left list + cursor, right status panel, `active:` strip above footer.
- **Catalog** (`menu_catalog.*`): static labels/actions. Nav dispatches `sub_action_t`; screens only paint.

Encoder: turn = move selection; click = activate. **Back** leaves submenu; HID/AirSpam can keep running.

## BLE sharing

One NimBLE host. HID owns stack bring-up; AirSpam reuses it.

- HID: connectable adv as `DEDSEC KBD` (`esp_hid` + report map).
- AirSpam: non-connectable Apple manufacturer payloads. **Not** in scan lists — Continuity popups on iOS only.
- Mutual exclusion: starting AirSpam pauses HID adv; stop resumes if HID still active.

Never call `ble_gap_adv_stop()` before host sync — crash/reboot.

## Hardware gotchas

- TFT **DC on GPIO2** (strap): flaky upload. See `WIRING.md`.
- Flash tight (~4MB `SINGLE_APP_LARGE`); assets eat space.

## Assets

PNG → RGB565 headers under `display/assets/`. Source art in `images/`.
