#!/usr/bin/env python3
"""Generate Phantom wiring schematic PNG from board_pins.h / WIRING.md."""

from PIL import Image, ImageDraw, ImageFont

W, H = 1600, 1100
BG = (252, 252, 248)
FG = (20, 20, 30)
RED = (180, 40, 40)
GRN = (30, 120, 60)
BLU = (30, 80, 160)
ORG = (200, 100, 20)
GRY = (120, 120, 130)
BOX_ESP = (235, 245, 255)
BOX_TFT = (255, 245, 230)
BOX_ENC = (240, 255, 240)


def font(size: int, bold: bool = False):
    names = [
        "C:/Windows/Fonts/consolab.ttf" if bold else "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/segoeuib.ttf" if bold else "C:/Windows/Fonts/segoeui.ttf",
        "arial.ttf",
    ]
    for name in names:
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def box(draw, xy, title, lines, fill, title_col=FG):
    x0, y0, x1, y1 = xy
    draw.rounded_rectangle(xy, radius=12, fill=fill, outline=FG, width=2)
    draw.text((x0 + 14, y0 + 10), title, fill=title_col, font=font(22, True))
    y = y0 + 42
    for line in lines:
        draw.text((x0 + 14, y), line, fill=FG, font=font(16))
        y += 22


def wire(draw, p1, p2, color, label=None, label_at=0.5):
    draw.line([p1, p2], fill=color, width=3)
    if label:
        lx = p1[0] + (p2[0] - p1[0]) * label_at
        ly = p1[1] + (p2[1] - p1[1]) * label_at - 10
        draw.text((lx, ly), label, fill=color, font=font(13, True))


def main():
    img = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(img)

    draw.text((40, 24), "Phantom — ESP32-S NodeMCU Wiring Schematic", fill=FG, font=font(30, True))
    draw.text((40, 62), "ILI9341 2.8\" SPI TFT 240×320 v1.2  +  KY-040 Rotary Encoder  |  pins: board_pins.h", fill=GRY, font=font(15))

    # Blocks
    esp = (60, 130, 420, 620)
    tft = (980, 110, 1540, 520)
    enc = (980, 580, 1540, 900)
    pwr = (60, 680, 420, 860)

    box(draw, esp, "ESP32-S NodeMCU", [
        "USB → 5V / 3V3 regulator",
        "SPI2_HOST (HSPI)",
        "",
        "GPIO15  CS",
        "GPIO18  SCK",
        "GPIO23  MOSI",
        "GPIO2   DC  ⚠ strap",
        "GPIO4   RST",
        "GPIO21  BL (optional)",
        "",
        "GPIO25  ENC CLK",
        "GPIO26  ENC DT",
        "GPIO27  ENC SW",
        "",
        "3V3  ·  GND  ·  EN  ·  BOOT",
    ], BOX_ESP)

    box(draw, tft, "2.8\" TFT ILI9341 (SPI)", [
        "VCC  → 3.3V",
        "GND  → GND",
        "CS   → GPIO15",
        "RESET→ GPIO4",
        "DC/RS→ GPIO2",
        "SDI  → GPIO23 (MOSI)",
        "SCK  → GPIO18",
        "LED  → 3.3V (hardwire)",
        "",
        "320×240 RGB565",
        "40 MHz SPI",
    ], BOX_TFT, BLU)

    box(draw, enc, "KY-040 Rotary Encoder", [
        "+    → 3.3V (not 5V!)",
        "GND  → GND",
        "CLK  → GPIO25  (rotate A)",
        "DT   → GPIO26  (rotate B)",
        "SW   → GPIO27  (click)",
        "",
        "Turn = menu nav",
        "Click = select / back",
    ], BOX_ENC, GRN)

    box(draw, pwr, "Power Notes", [
        "• TFT + WiFi: use good USB data cable",
        "• Weak USB → brownout reset",
        "• Optional: 5V on VIN + powered hub",
        "• BLE/WiFi share 2.4 GHz antenna",
    ], (255, 250, 240), ORG)

    # Pin anchor points on ESP block (right side)
    esp_pins = {
        "3V3": (420, 180),
        "GND": (420, 210),
        "15": (420, 260),
        "18": (420, 290),
        "23": (420, 320),
        "2": (420, 350),
        "4": (420, 380),
        "21": (420, 410),
        "25": (420, 470),
        "26": (420, 500),
        "27": (420, 530),
    }

    tft_pins = {
        "VCC": (980, 160),
        "GND": (980, 190),
        "CS": (980, 220),
        "RST": (980, 250),
        "DC": (980, 280),
        "MOSI": (980, 310),
        "SCK": (980, 340),
        "LED": (980, 370),
    }

    enc_pins = {
        "VCC": (980, 630),
        "GND": (980, 660),
        "CLK": (980, 690),
        "DT": (980, 720),
        "SW": (980, 750),
    }

    # TFT wires
    wire(draw, esp_pins["3V3"], tft_pins["VCC"], RED, "3V3")
    wire(draw, esp_pins["GND"], tft_pins["GND"], FG, "GND")
    wire(draw, esp_pins["15"], tft_pins["CS"], BLU, "CS")
    wire(draw, esp_pins["4"], tft_pins["RST"], BLU, "RST")
    wire(draw, esp_pins["2"], tft_pins["DC"], ORG, "DC ⚠")
    wire(draw, esp_pins["23"], tft_pins["MOSI"], BLU, "MOSI")
    wire(draw, esp_pins["18"], tft_pins["SCK"], BLU, "SCK")
    wire(draw, esp_pins["3V3"], tft_pins["LED"], RED, "BL→3V3")

    # Encoder wires
    wire(draw, esp_pins["3V3"], enc_pins["VCC"], RED, "3V3")
    wire(draw, esp_pins["GND"], enc_pins["GND"], FG, "GND")
    wire(draw, esp_pins["25"], enc_pins["CLK"], GRN, "CLK")
    wire(draw, esp_pins["26"], enc_pins["DT"], GRN, "DT")
    wire(draw, esp_pins["27"], enc_pins["SW"], GRN, "SW")

    # Legend
    lx, ly = 60, 920
    draw.rounded_rectangle((lx, ly, lx + 520, ly + 150), radius=10, fill=(245, 245, 245), outline=GRY, width=1)
    draw.text((lx + 12, ly + 8), "Legend", font=font(16, True), fill=FG)
    items = [(RED, "Power 3V3"), (FG, "Ground"), (BLU, "SPI / digital"), (GRN, "Encoder"), (ORG, "Boot strap (GPIO2)")]
    for i, (col, txt) in enumerate(items):
        yy = ly + 38 + i * 22
        draw.rectangle((lx + 16, yy + 4, lx + 36, yy + 16), fill=col)
        draw.text((lx + 44, yy), txt, font=font(14), fill=FG)

    # Warnings
    draw.rounded_rectangle((620, 130, 940, 360), radius=10, fill=(255, 240, 240), outline=RED, width=2)
    draw.text((640, 145), "⚠ Upload / Boot", font=font(18, True), fill=RED)
    warn = [
        "GPIO2 = DC + boot strap.",
        "TFT chatter breaks flash.",
        "",
        "If upload fails:",
        "1. Hold BOOT, tap EN",
        "2. Unplug DC from P2",
        "3. Close serial monitor",
    ]
    y = 175
    for line in warn:
        draw.text((640, y), line, font=font(14), fill=FG)
        y += 22

    draw.rounded_rectangle((620, 390, 940, 620), radius=10, fill=(240, 248, 255), outline=BLU, width=2)
    draw.text((640, 405), "On-board features", font=font(18, True), fill=BLU)
    feats = [
        "• BLE HID keyboard",
        "• Apple Spam (Continuity)",
        "• BLE device clone",
        "• WiFi evil twin / captive",
        "• No extra wiring needed",
    ]
    y = 435
    for line in feats:
        draw.text((640, y), line, font=font(14), fill=FG)
        y += 24

    draw.text((40, H - 36), "Generated from WIRING.md · ESP-IDF / PlatformIO · Phantom", fill=GRY, font=font(13))

    out = "docs/phantom_schematic.png"
    img.save(out, "PNG")
    print(f"Wrote {out} ({W}x{H})")


if __name__ == "__main__":
    main()
