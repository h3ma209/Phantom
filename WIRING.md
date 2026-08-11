# ESP32-S NodeMCU ("P" labels) ↔ 2.8" TFT SPI 240x320 v1.2 (ILI9341)

Framework: **ESP-IDF** via PlatformIO (`framework = espidf`). Pins in `src/main.c` / `src/encoder.h`.

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
| **SW**  | **P27**(9
) | 27 | Click (press shaft) |
| **+**   | **3V3** | — | Power (3.3V, not 5V) |
| **GND** | **GND** | — | Ground |

Controls:
- **Turn** → move menu selection
- **Click (SW)** → open item / back from detail

## BT Exploits → HID Keyboard

BLE keyboard for **your** PC. Advertises as `DEDSEC KBD`.

1. Open **BT Exploits** → **HID Keyboard**
2. On PC: Bluetooth → add device → **DEDSEC KBD**
3. After pair, node types `hello from dedsec node`
4. Click again to retype. Turn encoder to go back.

If direction feels reversed, swap CLK↔DT wires or flip CW/CCW in `encoder.c`.

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
4. Orientation: `esp_lcd_panel_swap_xy` / `mirror` in `src/main.c`.
