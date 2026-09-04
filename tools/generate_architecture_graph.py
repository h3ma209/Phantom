#!/usr/bin/env python3
"""Generate Phantom architecture / flow graph PNG."""

from PIL import Image, ImageDraw, ImageFont

W, H = 1800, 1200
BG = (248, 250, 252)
FG = (18, 24, 38)
GRY = (110, 118, 130)
RED = (190, 55, 55)
BLU = (35, 90, 170)
GRN = (35, 130, 75)
ORG = (210, 110, 25)
PUR = (120, 70, 170)
CYAN = (20, 130, 150)


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


def rounded_box(draw, xy, title, lines, fill, border=FG, title_col=FG, title_size=17, line_size=13):
    x0, y0, x1, y1 = xy
    draw.rounded_rectangle(xy, radius=10, fill=fill, outline=border, width=2)
    draw.text((x0 + 12, y0 + 8), title, fill=title_col, font=font(title_size, True))
    y = y0 + 34
    for line in lines:
        draw.text((x0 + 12, y), line, fill=FG, font=font(line_size))
        y += 18


def arrow(draw, p1, p2, color=FG, width=2):
    draw.line([p1, p2], fill=color, width=width)
    x1, y1 = p2
    x0, y0 = p1
    if abs(x1 - x0) >= abs(y1 - y0):
        tip = (-8, -4) if x1 > x0 else (8, -4)
    else:
        tip = (-4, -8) if y1 > y0 else (-4, 8)
    draw.polygon([(x1, y1), (x1 + tip[0], y1 + tip[1]), (x1 - tip[1], y1 + tip[0])], fill=color)


def center_bottom(box):
    x0, y0, x1, y1 = box
    return ((x0 + x1) // 2, y1)


def center_top(box):
    x0, y0, x1, y1 = box
    return ((x0 + x1) // 2, y0)


def center_right(box):
    x0, y0, x1, y1 = box
    return (x1, (y0 + y1) // 2)


def center_left(box):
    x0, y0, x1, y1 = box
    return (x0, (y0 + y1) // 2)


def main():
    img = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(img)

    draw.text((36, 20), "Phantom — System Architecture & Flow", fill=FG, font=font(32, True))
    draw.text((36, 58), "ESP32-S + ILI9341 TFT · ESP-IDF · PlatformIO", fill=GRY, font=font(15))

    # --- Boot chain (top) ---
    boot = [
        ((60, 95, 260, 155), "app_main", ["Firmware entry"], (230, 240, 255)),
        ((300, 95, 500, 155), "Hardware init", ["lcd_panel_init()", "rotary_encoder_init()"], (235, 248, 235)),
        ((540, 95, 740, 155), "Splash", ["menu_screens_draw_splash()", "glitch-out animation"], (255, 245, 230)),
        ((780, 95, 980, 155), "menu_nav_run()", ["Blocks forever — main loop"], (255, 235, 245)),
    ]
    boxes = {}
    for i, (xy, title, lines, fill) in enumerate(boot):
        rounded_box(draw, xy, title, lines, fill, border=BLU)
        boxes[f"boot{i}"] = xy
        if i:
            arrow(draw, center_right(boxes[f"boot{i-1}"]), center_left(xy), BLU)

    draw.text((1010, 112), "Boot path →", fill=BLU, font=font(14, True))

    # --- Core loop hub ---
    hub = (620, 200, 1120, 320)
    rounded_box(draw, hub, "menu_nav_run() — UI state machine (while loop)",
                [
                    "Screens: MAIN · SUB · LIST · RES · EVIL_BT",
                    "Each tick: wifi_manager_tick()",
                    "Encoder poll → navigate / activate / toggle",
                    "pause_ble() before WiFi scan or Fake AP",
                ], (255, 252, 220), border=ORG, title_col=ORG, title_size=18)
    arrow(draw, center_bottom(boxes["boot3"]), center_top(hub), ORG, 3)

    # --- Input / Output ---
    enc = (80, 220, 320, 340)
    disp = (1420, 220, 1680, 340)
    rounded_box(draw, enc, "Input — Rotary Encoder", [
        "GPIO25 CLK · GPIO26 DT · GPIO27 SW",
        "ISR → Gray code → FreeRTOS queue",
        "CW/CCW = move · Click = select",
    ], (235, 248, 255), border=BLU)
    rounded_box(draw, disp, "Output — ILI9341 Display", [
        "SPI 320×240 RGB565",
        "menu_screens / menu_list painters",
        "Partial redraws for scroll/focus",
    ], (235, 248, 255), border=BLU)
    arrow(draw, center_right(enc), center_left(hub), BLU)
    arrow(draw, center_right(hub), center_left(disp), BLU)

    # --- UI layer ---
    ui = (480, 360, 1260, 460)
    rounded_box(draw, ui, "UI layer (src/ui/)", [
        "menu_catalog — categories + actions   |   menu_screens — draw splash/main/sub/list/resources/evil-bt",
        "menu_nav — dispatch ACT_* actions     |   menu_list — shared WiFi scan / portal / log lists",
    ], (240, 244, 252), border=GRY)
    arrow(draw, (900, 320), (900, 360), GRY)

    # --- Main menu categories ---
    cats = (40, 500, 420, 720)
    rounded_box(draw, cats, "Main menu categories", [
        "Recon · Deauth (soon)",
        "Evil Twin — WiFi rogue AP",
        "Bluetooth Attacks — HID / Apple Spam",
        "Remote Control (soon)",
        "Evil Bluetooth — scan + clone ads",
        "Resources — heap / flash / CPU",
    ], (245, 240, 255), border=PUR, title_col=PUR)
    arrow(draw, center_bottom(ui), (230, 500), PUR)

    # --- WiFi branch ---
    wifi_mgr = (480, 500, 780, 640)
    rounded_box(draw, wifi_mgr, "wifi_manager (sole esp_wifi owner)", [
        "OFF / SCAN / AP modes — switch stops prior",
        "Clone SSID from scan cache",
        "Fake AP @ 172.217.28.1",
    ], (255, 240, 240), border=RED, title_col=RED)

    wifi_mod = (480, 660, 780, 820)
    rounded_box(draw, wifi_mod, "WiFi modules (src/wifi/)", [
        "wifi_scan — STA scan cache",
        "wifi_ap — soft AP + HTTP server",
        "wifi_dns — UDP DNS hijack (* → AP)",
        "wifi_portals — 4 HTML templates",
        "wifi_portal_log — captured creds",
    ], (255, 248, 248), border=RED)

    phone = (480, 860, 780, 980)
    rounded_box(draw, phone, "Victim phone / laptop", [
        "Joins open cloned SSID",
        "DNS → captive portal page",
        "Credentials logged on device",
    ], (252, 252, 252), border=GRY)

    arrow(draw, center_right(cats), center_left(wifi_mgr), RED)
    arrow(draw, center_bottom(wifi_mgr), center_top(wifi_mod), RED)
    arrow(draw, center_bottom(wifi_mod), center_top(phone), RED, 2)
    draw.text((790, 555), "Evil Twin", fill=RED, font=font(13, True))

    # --- BLE branch ---
    ble_hub = (920, 500, 1240, 620)
    rounded_box(draw, ble_hub, "Shared NimBLE host (one stack)", [
        "ble_hid_gap + ble_hid_keyboard bring-up",
        "Only one feature advertises at a time",
        "Mutual pause: HID / Apple Spam / clone",
    ], (235, 248, 255), border=BLU, title_col=BLU)

    ble_feats = (920, 640, 1240, 860)
    rounded_box(draw, ble_feats, "BLE features (src/features/)", [
        "Keyboard — HID peripheral PHANTOM KBD",
        "Apple Spam — Continuity popup flood",
        "ble_clone — scan named devices, clone adv",
    ], (240, 248, 255), border=BLU)

    targets = (920, 880, 1240, 980)
    rounded_box(draw, targets, "Targets", [
        "PC pairs as BT keyboard",
        "iPhone — Apple TV / AirPods popups",
        "Nearby phones see cloned BLE name",
    ], (252, 252, 252), border=GRY)

    arrow(draw, (420, 610), center_left(ble_hub), BLU)
    arrow(draw, center_bottom(ble_hub), center_top(ble_feats), BLU)
    arrow(draw, center_bottom(ble_feats), center_top(targets), BLU, 2)
    draw.text((1250, 545), "BT / Evil BT", fill=BLU, font=font(13, True))

    # --- Evil BT screen ---
    ebt = (1280, 500, 1680, 640)
    rounded_box(draw, ebt, "SCR_EVIL_BT screen", [
        "ble_clone_start_scan() — find BLE names",
        "Pick device → clone its advertisement",
        "Random MAC + replay adv payload",
    ], (245, 240, 255), border=PUR, title_col=PUR)
    arrow(draw, center_right(cats), center_left(ebt), PUR)

    # --- Resources ---
    res = (1280, 680, 1680, 780)
    rounded_box(draw, res, "SCR_RES — Resources", [
        "Free heap · DRAM · flash · task count",
        "Live refresh while viewing",
    ], (240, 252, 245), border=GRN, title_col=GRN)
    arrow(draw, (420, 680), center_left(res), GRN)

    # --- Rules box ---
    rules = (1280, 820, 1680, 980)
    rounded_box(draw, rules, "Key rules", [
        "UI never calls esp_wifi_* directly",
        "WiFi start → pause_ble()",
        "Do not adv_stop before BLE host sync",
        "HID/Apple Spam stay armed on Back",
        "Lab / authorized use only",
    ], (255, 252, 235), border=ORG, title_col=ORG)

    # --- Data flow legend ---
    leg = (40, 760, 420, 980)
    draw.rounded_rectangle(leg, radius=10, fill=(245, 247, 250), outline=GRY, width=1)
    draw.text((52, 772), "Flow legend", font=font(16, True), fill=FG)
    items = [
        (BLU, "Boot / I/O / BLE"),
        (ORG, "Main control loop"),
        (RED, "WiFi evil twin path"),
        (PUR, "Menu navigation"),
        (GRN, "Diagnostics"),
    ]
    for i, (col, txt) in enumerate(items):
        yy = 802 + i * 28
        draw.rectangle((60, yy + 5, 88, yy + 19), fill=col)
        draw.text((96, yy), txt, font=font(14), fill=FG)

    draw.text((36, H - 32), "See ARCHITECTURE.md · regenerate: python tools/generate_architecture_graph.py",
              fill=GRY, font=font(13))

    out = "docs/phantom_architecture.png"
    img.save(out, "PNG")
    print(f"Wrote {out} ({W}x{H})")


if __name__ == "__main__":
    main()
