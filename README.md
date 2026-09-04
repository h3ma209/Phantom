# Phantom

Handheld ESP32 pentest node with a **320×240 ILI9341 TFT** and **rotary encoder** menu. WiFi evil-twin tooling, BLE HID keyboard, Apple Continuity spam, and BLE advertisement cloning — all from a single knob-driven UI.

Built with **PlatformIO** and **ESP-IDF** (not Arduino).

> **Lab / authorized networks only.** Deauth is not implemented. Use responsibly.

## Hardware

| Part | Notes |
|------|--------|
| ESP32-S NodeMCU v1.1 | USB power; BLE + WiFi on 2.4 GHz |
| 2.8" SPI TFT ILI9341 v1.2 | 240×320 panel, landscape UI |
| KY-040 rotary encoder | Turn = navigate, click = select |

**Wiring:** see [WIRING.md](WIRING.md)  
**Schematic:** [docs/phantom_schematic.png](docs/phantom_schematic.png)  
**Architecture:** [docs/phantom_architecture.png](docs/phantom_architecture.png) · [ARCHITECTURE.md](ARCHITECTURE.md)

Pin map lives in `src/board/board_pins.h` and `src/input/rotary_encoder.h`.

## Features

| Menu | Status | Description |
|------|--------|-------------|
| **Evil Twin** | Working | Scan APs → clone SSID → open Fake AP + DNS hijack + captive portal |
| **Bluetooth Attacks → Keyboard** | Working | BLE HID peripheral (`PHANTOM KBD`) — pair with PC and type demo text |
| **Bluetooth Attacks → Apple Spam** | Working | Continuity popup flood (AirPods, Apple TV, HomePod, etc.) |
| **Evil Bluetooth** | Working | Scan named BLE devices and clone their advertisements |
| **Resources** | Working | Live heap, flash, and task stats |
| Recon / Deauth / Remote / Jammer | Placeholder | Shown in menu, not implemented yet |

### Evil Twin flow

1. **Clone AP** — scan nearby networks, pick an SSID to copy.
2. **Captive Portal** — choose an HTML template (IQ, Komar, MyKomar, Komar Cap).
3. **Fake AP** — toggle on. Victim joins open AP; DNS redirects to portal at `172.217.28.1`.
4. **Portal Log** — view credentials captured on the device.

### Apple Spam

Floods Apple Continuity BLE packets with rotating MAC addresses so dismissed popups can reappear. Targets iPhone with **Settings → Bluetooth → On**, phone unlocked. Does not show up in normal BT scan lists.

## Controls

- **Rotate** — move selection
- **Click** — open category / activate item / toggle Fake AP
- **Back** (last submenu row) — return to main menu

HID keyboard and Apple Spam stay armed when you go Back from the Bluetooth submenu. WiFi operations pause BLE automatically.

## Build & flash

Requires [PlatformIO](https://platformio.org/).

```bash
pio run              # build
pio run -t upload    # flash
pio device monitor   # serial log @ 115200
```

Set your serial port in `platformio.ini` (`upload_port`) or pass `-upload-port COMx`.

### Upload gotcha

TFT **DC is on GPIO2** (boot strap pin). If upload fails with `Invalid head of packet`:

1. Close the serial monitor.
2. Hold **BOOT** → tap **EN** → release **EN**, keep **BOOT** until `Connecting...`.
3. Still failing: unplug TFT **DC** from **P2**, upload, reconnect.
4. Use a short **data** USB cable (not charge-only).

### Blank screen

Wire TFT **LED** directly to **3V3** for backlight. See [WIRING.md](WIRING.md).

### Brownout on WiFi

WiFi scan/AP draws a current spike. Weak USB can reset the board back to the main menu. Use a powered hub or 5V on **VIN**.

## Project layout

```
src/
  board/       Pin definitions
  display/     ILI9341 driver, draw helpers, RGB565 assets
  input/       Rotary encoder (ISR → queue)
  ui/          Menu catalog, screens, navigation, lists
  wifi/        wifi_manager, scan, AP, DNS, portals
  features/    BLE HID keyboard, Apple Spam, BLE clone
docs/          Schematic and architecture diagrams
tools/         Asset compression, diagram generators
```

## Regenerate docs

```bash
python tools/generate_schematic.py
python tools/generate_architecture_graph.py
```

## License

See component licenses under `managed_components/`. Application code: use at your own risk on networks you own or have permission to test.
