# ESP32-S NodeMCU ("P" labels) ↔ 2.8" TFT SPI 240x320 v1.2 (ILI9341)

Framework: **ESP-IDF** via PlatformIO (`framework = espidf`).
Pins: `src/board/board_pins.h` (TFT), `src/input/rotary_encoder.h` (encoder).
Software map: `ARCHITECTURE.md`.

## TFT

| TFT pin     | Board label | GPIO | Notes |
|-------------|-------------|------|-------|
| VCC         | **3V3**     | —    | Prefer 3.3V |
| GND         | **GND**     | —    | |
| CS          | **P15**(4)     | 15   | Strapping — OK after boot |
| RESET       | **P4**  (7)    | 4    | Do not leave floating |
| DC / RS     | **P2**   (5)   | 2    | Strapping / onboard LED |
| SDI (MOSI)  | **P23**(18)     | 23   | |
| SCK         | **P18**  (11)   | 18   | |
| LED         | **3V3**     | —    | **Prefer hardwire to 3V3** (always on). P21 optional PWM later |

## Rotary encoder (KY-040 style)

| Encoder | Board | GPIO | Role |
|---------|-------|------|------|
| **CLK** | **P25**(11) | 25 | Rotate A |
| **DT**  | **P26**(10) | 26 | Rotate B |
| **SW**  | **P27**(9) | 27 | Click (press shaft) |
| **+**   | **3V3** | — | Power (3.3V, not 5V) |
| **GND** | **GND** | — | Ground |

Controls:
- **Turn** → move menu selection
- **Click (SW)** → open item / activate; **Back** returns to main

## Bluetooth Attacks → HID Keyboard

BLE keyboard for **your** PC. Advertises as `DEDSEC KBD`.

1. Open **Bluetooth Attacks** → **Keyboard**
2. On PC: Bluetooth → add device → **DEDSEC KBD**
3. After pair, node types `hello from dedsec node`
4. Click again to retype. **Back** leaves submenu (adv can keep running).

If direction feels reversed, swap CLK↔DT wires or flip CW/CCW in `src/input/rotary_encoder.c`.

## Bluetooth Attacks → AirSpam

Apple Continuity spam. **Will not show in BT scan lists.**

Test tips:
1. iPhone: **Settings → Bluetooth → On** (Control Center alone is not enough).
2. Phone unlocked, screen on; not in Airplane mode; close Camera/Keyboard if open.
3. Expect **AppleTV / Setup / Color Balance** banners more than AirPods on recent iOS.
4. If quiet: lock → unlock once (iOS rate-limits hard).
5. Stay within ~1–2 m of the ESP32.

Toggle stops advertising; HID adv resumes if keyboard still armed.

## Evil Twin (WiFi)

Pengz0-style Fake AP on this firmware (ESP-IDF, not Arduino):

1. **Evil Twin → Clone AP** — scan, click SSID to copy the name.
2. **Captive Portal** — pick HTML template.
3. **Fake AP** — toggle on. Phone joins open AP, captive page loads. IP `172.217.28.1`.
4. Toggle Fake AP again to stop.

Lab / networks you own only. Deauth is not implemented.

**Power:** WiFi scan/AP draws a current spike. Weak USB → brownout reset (list flashes, back to main menu). Use a **short data USB cable**, **powered hub**, or **5V on VIN**. Firmware lowers TX power to help; still may need better supply.

## Upload

TFT **DC is on P2 (GPIO2)** — boot strap pin. Causes `Invalid head of packet` / chip stop if TFT still talking during flash.

```bash
cd tftscreen
pio run -t upload
```

If upload fails:

1. Close Serial Monitor (port busy / leftover UART junk).
2. Hold **BOOT** → tap **EN** → keep BOOT held until `Connecting...` succeeds.
3. Still fail: unplug TFT **DC** from **P2** (or power TFT off), upload, plug DC back.
4. Use a short data USB cable (not charge-only).

## If blank screen

1. **Backlight first:** TFT **LED** → **3V3** (most common blank cause).
2. Confirm TFT rows — especially **P23** MOSI (right side), not P22.
3. Swap check: CS / DC / RST.
4. Orientation: `esp_lcd_panel_swap_xy` / `mirror` in `src/display/lcd_panel.c`.
