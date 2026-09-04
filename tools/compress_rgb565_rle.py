#!/usr/bin/env python3
"""RLE-compress RGB565 asset headers (e.g. splash)."""
import re
import struct
import sys
from pathlib import Path


def parse_rgb565_header(path: Path):
    text = path.read_text(encoding="utf-8")
    wm = re.search(r"#define\s+(\w+)_W\s+(\d+)", text)
    hm = re.search(r"#define\s+(\w+)_H\s+(\d+)", text)
    if not wm or not hm:
        raise SystemExit(f"no W/H defines in {path}")
    prefix = wm.group(1)
    w, h = int(wm.group(2)), int(hm.group(2))
    vals = [int(x, 0) for x in re.findall(r"0x[0-9a-fA-F]+", text.split("=", 1)[1])]
    if len(vals) != w * h:
        raise SystemExit(f"{path.name}: expected {w*h} pixels, got {len(vals)}")
    return prefix, w, h, vals


def rle_encode_rgb565(pixels):
    out = bytearray()
    i = 0
    n = len(pixels)
    while i < n:
        px = pixels[i]
        run = 1
        while i + run < n and pixels[i + run] == px and run < 65535:
            run += 1
        out += struct.pack("<HH", run, px)
        i += run
    return bytes(out)


def emit_rle_header(prefix: str, w: int, h: int, raw: list, compressed: bytes, out: Path):
    name = prefix.upper()
    ratio = 100 * len(compressed) / (w * h * 2)
    lines = [
        "#pragma once",
        "#include <stdint.h>",
        "",
        f"/* RLE RGB565 — {w}x{h}. Raw {w*h*2} B -> {len(compressed)} B ({ratio:.1f}%). */",
        f"#define {name}_W {w}",
        f"#define {name}_H {h}",
        f"#define {name}_RLE_LEN {len(compressed)}",
        "",
        f"static const uint8_t {name}_RLE_DATA[{len(compressed)}] = {{",
    ]
    row = "    "
    for i, b in enumerate(compressed):
        row += f"0x{b:02x},"
        if len(row) > 100:
            lines.append(row)
            row = "    "
    if row.strip():
        lines.append(row)
    lines.append("};")
    lines.append("")
    out.write_text("\n".join(lines), encoding="utf-8")


def main():
    root = Path(__file__).resolve().parents[1]
    src = root / "src" / "display" / "assets" / "asset_splash.h"
    if len(sys.argv) > 1:
        src = Path(sys.argv[1])
    prefix, w, h, pixels = parse_rgb565_header(src)
    compressed = rle_encode_rgb565(pixels)
    out = src.with_name(src.stem + "_rle.h")
    emit_rle_header(prefix, w, h, pixels, compressed, out)
    print(f"{src.name}: {w*h*2} -> {len(compressed)} bytes ({100*len(compressed)/(w*h*2):.1f}%)")
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
