#!/usr/bin/env python3
"""Build the small Material Symbols font used by the LAN model drop sender page.

    python3 scripts/md3/subset_lan_drop_icons.py

The sender page is served from a container on the local network to phones, so
it carries only the icons it shows instead of the whole 4 MB variable font.
The subset keeps the ligature names (``<span data-icon>send</span>`` still
works) and the FILL axis; weight, grade and optical size are fixed at the
design system's defaults (400, 0, 24). Needs fontTools with Brotli
(``pip install fonttools brotli``); the result is committed, so building the
container never needs it.
"""

from __future__ import annotations

import io
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SOURCE = ROOT / "ui-md3" / "assets" / "fonts" / "MaterialSymbolsOutlined.woff2"
TARGET = ROOT / "lan-model-drop" / "site" / "fonts" / "MaterialSymbolsOutlined-subset.woff2"

# Every icon name the sender page uses. tests/lan_model_drop/site.test.mjs
# checks that this list and the page agree.
ICONS = (
    "block",
    "check_circle",
    "close",
    "computer",
    "delete_sweep",
    "deployed_code",
    "error",
    "hourglass_top",
    "info",
    "lock",
    "person",
    "pin",
    "progress_activity",
    "send",
    "upload_file",
    "view_in_ar",
)


def main() -> int:
    try:
        from fontTools import subset
        from fontTools.ttLib import TTFont
        from fontTools.varLib import instancer
    except ImportError:
        print("fontTools is required: pip install fonttools brotli", file=sys.stderr)
        return 2

    font = TTFont(str(SOURCE))
    names = set(font.getGlyphOrder())
    missing = [icon for icon in ICONS if icon not in names]
    if missing:
        print(f"not in the source font: {', '.join(missing)}", file=sys.stderr)
        return 1
    font = instancer.instantiateVariableFont(font, {"GRAD": 0, "opsz": 24, "wght": 400})
    # Round-trip the partial instance so every table is compiled before
    # subsetting (glyphs without remaining variations are then consistent).
    buffer = io.BytesIO()
    font.save(buffer)
    buffer.seek(0)
    font = TTFont(buffer, lazy=False)
    glyphs = list(ICONS) + [f"{icon}.fill" for icon in ICONS if f"{icon}.fill" in names]

    options = subset.Options()
    options.layout_features = ["*"]
    options.layout_closure = False
    options.notdef_outline = True
    options.name_IDs = ["*"]
    options.name_languages = ["*"]
    options.flavor = "woff2"
    subsetter = subset.Subsetter(options)
    # The ligature components are the letters of the icon names.
    subsetter.populate(glyphs=glyphs, text="".join(sorted(set("".join(ICONS)))))
    subsetter.subset(font)
    font.flavor = "woff2"
    TARGET.parent.mkdir(parents=True, exist_ok=True)
    font.save(str(TARGET))
    print(f"wrote {TARGET.relative_to(ROOT).as_posix()} ({TARGET.stat().st_size} bytes, {len(glyphs)} icon glyphs)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
