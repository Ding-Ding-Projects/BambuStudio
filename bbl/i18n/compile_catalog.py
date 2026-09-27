#!/usr/bin/env python3
"""Compile a fork-owned override catalog (the English wording layer) into a deterministic MO.

    py -3 bbl/i18n/compile_catalog.py --po bbl/i18n/en/BambuStudio_en.po --output <dir>/en/BambuStudio.mo

The English catalog is not a translation: it overrides upstream msgids with the
fork's wording (ink terminology, copy edits such as "Cancle" -> "Cancel").
Entries with an empty or fuzzy msgstr are left out, so wxWidgets falls back to
the msgid for them. The build compiles this at build time and installs it over
the upstream-tracked resources/i18n/en/BambuStudio.mo; see CMakeLists.txt.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))

from po_catalog import CatalogError, compile_mo, entry_map, mo_original, mo_translation, parse_po, read_mo  # noqa: E402


def compile_override(po_path: Path) -> bytes:
    entries = entry_map(parse_po(po_path), po_path, allow_duplicates=True)
    header = entries.get((None, ""))
    if header is None or "charset=UTF-8" not in header.msgstr:
        raise CatalogError(f"{po_path}: header must declare charset=UTF-8")
    pairs = [("", header.msgstr)]
    for entry in entries.values():
        if entry.is_header or entry.fuzzy or not entry.translated():
            continue
        pairs.append((mo_original(entry), mo_translation(entry)))
    compiled = compile_mo(pairs)
    decoded = read_mo(compiled)
    for original, translation in pairs:
        if decoded.get(original) != translation:
            raise CatalogError(f"compiled catalog does not round-trip: {original!r}")
    return compiled


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--po", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--check", action="store_true", help="write nothing; fail if --output exists and differs")
    args = parser.parse_args()
    try:
        compiled = compile_override(args.po)
        if args.check:
            if args.output.is_file() and args.output.read_bytes() != compiled:
                raise CatalogError(f"compiled catalog is stale: {args.output}")
        else:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_bytes(compiled)
    except (CatalogError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    print(f"{'Checked' if args.check else 'Wrote'} {args.output} ({len(compiled)} bytes).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
