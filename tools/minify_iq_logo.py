#!/usr/bin/env python3
"""Build compact IQ Online logo for captive portal (/iq.svg)."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "iQ Online Logo (1).svg"
OUT_H = ROOT / "src" / "wifi" / "wifi_portal_iq_logo.h"
OUT_C = ROOT / "src" / "wifi" / "wifi_portal_iq_logo.c"


def compact_nums(path: str, prec: int = 1) -> str:
    out = []
    for tok in re.split(r"(\d+\.\d+)", path):
        if re.fullmatch(r"\d+\.\d+", tok):
            v = round(float(tok), prec)
            if v == int(v):
                out.append(str(int(v)))
            else:
                out.append(f"{v:.{prec}f}".rstrip("0").rstrip("."))
        else:
            out.append(tok)
    return "".join(out)


def build_svg(raw: str) -> str:
    svg = re.sub(r"\s+", " ", raw).strip()
    svg = re.sub(r"<g clip-path=\"[^\"]+\">", "", svg)
    svg = svg.replace("</g>", "")
    svg = re.sub(r"<defs>.*?</defs>", "", svg)
    m = re.search(r'd="([^"]+)"', svg)
    if not m:
        raise SystemExit("no path in SVG")
    path = compact_nums(m.group(1))
    return (
        '<svg viewBox="0 0 83 133">'
        f'<path fill="#fff" d="{path}"/>'
        "</svg>"
    )


def emit_c_string(s: str) -> str:
    lines = ['const char WIFI_PORTAL_IQ_LOGO_SVG[] =']
    chunk = 96
    for i in range(0, len(s), chunk):
        part = s[i : i + chunk].replace("\\", "\\\\").replace('"', '\\"')
        lines.append(f'    "{part}"')
    lines.append(";")
    return "\n".join(lines)


def main() -> None:
    raw = SRC.read_text(encoding="utf-8")
    svg = build_svg(raw)
    print(f"SVG {len(raw)} -> {len(svg)} bytes ({100 * len(svg) / len(raw):.0f}%)")

    OUT_H.write_text(
        "#pragma once\n\n"
        "#include <stddef.h>\n\n"
        "extern const char WIFI_PORTAL_IQ_LOGO_SVG[];\n"
        "extern const size_t WIFI_PORTAL_IQ_LOGO_SVG_LEN;\n",
        encoding="utf-8",
    )
    OUT_C.write_text(
        '#include "wifi_portal_iq_logo.h"\n\n'
        f"{emit_c_string(svg)}\n\n"
        "const size_t WIFI_PORTAL_IQ_LOGO_SVG_LEN = sizeof(WIFI_PORTAL_IQ_LOGO_SVG) - 1;\n",
        encoding="utf-8",
    )
    print(f"wrote {OUT_H.name}, {OUT_C.name}")


if __name__ == "__main__":
    main()
