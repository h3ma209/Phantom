#!/usr/bin/env python3
"""Export RGB565 C headers to PNG for the GitHub Pages site."""
from __future__ import annotations

import re
import shutil
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ASSETS_DIR = ROOT / "docs" / "assets"
SRC = ROOT / "src" / "display" / "assets"


def parse_rgb565_header(path: Path) -> tuple[int, int, list[int]]:
    text = path.read_text(encoding="utf-8")
    wm = re.search(r"#define\s+\w+_W\s+(\d+)", text)
    hm = re.search(r"#define\s+\w+_H\s+(\d+)", text)
    if not wm or not hm:
        raise SystemExit(f"no W/H defines in {path}")
    w, h = int(wm.group(1)), int(hm.group(1))
    body = text.split("=", 1)[1]
    vals = [int(x, 0) for x in re.findall(r"0x[0-9a-fA-F]+", body)]
    if len(vals) != w * h:
        raise SystemExit(f"{path.name}: expected {w * h} pixels, got {len(vals)}")
    return w, h, vals


def rgb565_to_rgba(px: int, key: int | None) -> tuple[int, int, int, int]:
    if key is not None and px == key:
        return (0, 0, 0, 0)
    r = ((px >> 11) & 0x1F) * 255 // 31
    g = ((px >> 5) & 0x3F) * 255 // 63
    b = (px & 0x1F) * 255 // 31
    return (r, g, b, 255)


def export(src: Path, dest: Path, key: int | None = None) -> None:
    w, h, vals = parse_rgb565_header(src)
    img = Image.new("RGBA", (w, h))
    img.putdata([rgb565_to_rgba(p, key) for p in vals])
    dest.parent.mkdir(parents=True, exist_ok=True)
    img.save(dest, "PNG")
    print(f"{src.name} -> {dest.relative_to(ROOT)} ({w}x{h})")


def main() -> None:
    ASSETS_DIR.mkdir(parents=True, exist_ok=True)
    keyed = {
        "asset_icon_recon.h": "icon_recon.png",
        "asset_icon_deauth.h": "icon_deauth.png",
        "asset_icon_evil.h": "icon_evil.png",
        "asset_icon_bt.h": "icon_bt.png",
        "asset_icon_remote.h": "icon_remote.png",
        "asset_cursor.h": "cursor.png",
    }
    opaque = {
        "asset_splash.h": "splash.png",
        "asset_menu_bg.h": "menu_bg.png",
        "asset_submenu_bg.h": "submenu_bg.png",
    }
    for src_name, dest_name in keyed.items():
        export(SRC / src_name, ASSETS_DIR / dest_name, key=0x0000)
    for src_name, dest_name in opaque.items():
        export(SRC / src_name, ASSETS_DIR / dest_name)

    pinout = ROOT / "pinout-809.jpg"
    if pinout.exists():
        dest = ASSETS_DIR / "pinout.jpg"
        shutil.copy2(pinout, dest)
        print(f"{pinout.name} -> {dest.relative_to(ROOT)}")

    fav = Image.open(ASSETS_DIR / "icon_evil.png")
    fav.resize((32, 32), Image.NEAREST).save(ASSETS_DIR / "favicon.png")
    print("favicon.png 32x32")


if __name__ == "__main__":
    main()
