#!/usr/bin/env python3
"""Validate the reviewed yue_HK catalog and compile a deterministic GNU MO."""

from __future__ import annotations

import argparse
import ast
from collections import Counter
import json
from pathlib import Path
import re
import struct
import sys
from typing import Dict, List, Tuple


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[2]
DEFAULT_PO = SCRIPT_DIR / "BambuStudio_yue_HK.po"
DEFAULT_SOURCE = SCRIPT_DIR.parent / "en" / "BambuStudio_en.po"
DEFAULT_COVERAGE = SCRIPT_DIR / "coverage.json"
DEFAULT_OUTPUT = REPO_ROOT / "resources" / "i18n" / "yue_HK" / "BambuStudio.mo"

PLACEHOLDER_RE = re.compile(
    r"%\d+%"
    r"|%%"
    r"|%(?:\d+\$)?[-+#0 'I]*(?:\d+|\*)?(?:\.(?:\d+|\*))?"
    r"(?:hh|h|ll|l|j|z|t|L)?[diuoxXfFeEgGaAcspn]"
    r"|\{\{\s*[^{}]+\s*\}\}"
    r"|\{[A-Za-z_][^{}]*\}"
)


class CatalogError(ValueError):
    pass


def _quoted(value: str, path: Path, line_number: int) -> str:
    try:
        parsed = ast.literal_eval(value)
    except (SyntaxError, ValueError) as exc:
        raise CatalogError(f"{path}:{line_number}: invalid PO string: {exc}") from exc
    if not isinstance(parsed, str):
        raise CatalogError(f"{path}:{line_number}: PO value is not a string")
    return parsed


def parse_po(path: Path) -> List[Dict[str, object]]:
    entries: List[Dict[str, object]] = []
    current: Dict[str, object] = {"categories": []}
    active: str | None = None

    def finish() -> None:
        nonlocal current, active
        if "msgid" in current:
            entries.append(current)
        current = {"categories": []}
        active = None

    lines = path.read_text(encoding="utf-8").splitlines()
    for line_number, line in enumerate([*lines, ""], 1):
        if not line.strip():
            finish()
            continue
        if line.startswith("#. reviewed-category: "):
            categories = current["categories"]
            assert isinstance(categories, list)
            categories.append(line.removeprefix("#. reviewed-category: ").strip())
            continue
        if line.startswith("#"):
            continue

        match = re.match(r"(msgctxt|msgid|msgid_plural|msgstr(?:\[\d+\])?)\s+(\".*\")$", line)
        if match:
            active = match.group(1)
            current[active] = _quoted(match.group(2), path, line_number)
            continue
        if line.startswith('"') and active:
            previous = current.get(active, "")
            assert isinstance(previous, str)
            current[active] = previous + _quoted(line, path, line_number)
            continue
        raise CatalogError(f"{path}:{line_number}: unsupported PO syntax: {line}")

    return entries


def entry_map(
    entries: List[Dict[str, object]],
    path: Path,
    *,
    allow_duplicates: bool = False,
) -> Dict[str, Dict[str, object]]:
    result: Dict[str, Dict[str, object]] = {}
    for entry in entries:
        msgid = entry.get("msgid")
        if not isinstance(msgid, str):
            raise CatalogError(f"{path}: catalog entry has no msgid")
        if msgid in result:
            if allow_duplicates:
                continue
            raise CatalogError(f"{path}: duplicate msgid: {msgid!r}")
        result[msgid] = entry
    return result


def placeholder_signature(value: str) -> Counter[str]:
    return Counter(PLACEHOLDER_RE.findall(value))


def validate_catalog(po_path: Path, source_path: Path, coverage_path: Path) -> Dict[str, str]:
    target = entry_map(parse_po(po_path), po_path)
    # The upstream English extraction currently contains repeated msgids. They
    # represent the same lookup key, so collapse them only for source membership.
    source = entry_map(parse_po(source_path), source_path, allow_duplicates=True)

    header = target.get("")
    if not header or "Language: yue_HK\n" not in str(header.get("msgstr", "")):
        raise CatalogError("catalog header must declare Language: yue_HK")

    translated: Dict[str, str] = {}
    category_counts: Counter[str] = Counter()
    for msgid, entry in target.items():
        if msgid == "":
            continue
        msgstr = entry.get("msgstr")
        if not isinstance(msgstr, str) or not msgstr.strip():
            raise CatalogError(f"empty translation: {msgid!r}")
        if msgid not in source:
            raise CatalogError(f"msgid is absent from English source catalog: {msgid!r}")
        if placeholder_signature(msgid) != placeholder_signature(msgstr):
            raise CatalogError(
                f"placeholder mismatch for {msgid!r}: "
                f"{placeholder_signature(msgid)} != {placeholder_signature(msgstr)}"
            )
        categories = entry.get("categories", [])
        if not isinstance(categories, list) or len(categories) != 1:
            raise CatalogError(f"exactly one reviewed-category is required: {msgid!r}")
        category_counts[str(categories[0])] += 1
        translated[msgid] = msgstr

    coverage = json.loads(coverage_path.read_text(encoding="utf-8"))
    if coverage.get("translated_messages") != len(translated):
        raise CatalogError("coverage.json translated_messages does not match the PO catalog")
    if coverage.get("categories") != dict(sorted(category_counts.items())):
        raise CatalogError("coverage.json category counts do not match reviewed-category comments")
    if len(translated) < 150:
        raise CatalogError("at least 150 reviewed native translations are required")

    return {"": str(header["msgstr"]), **translated}


def compile_mo(catalog: Dict[str, str]) -> bytes:
    ordered: List[Tuple[bytes, bytes]] = sorted(
        ((msgid.encode("utf-8"), msgstr.encode("utf-8")) for msgid, msgstr in catalog.items()),
        key=lambda item: item[0],
    )
    count = len(ordered)
    originals_offset = 7 * 4
    translations_offset = originals_offset + count * 8
    strings_offset = translations_offset + count * 8

    original_blob = b"".join(msgid + b"\0" for msgid, _ in ordered)
    translation_blob = b"".join(msgstr + b"\0" for _, msgstr in ordered)

    original_table = bytearray()
    translation_table = bytearray()
    cursor = strings_offset
    for msgid, _ in ordered:
        original_table.extend(struct.pack("<II", len(msgid), cursor))
        cursor += len(msgid) + 1
    cursor = strings_offset + len(original_blob)
    for _, msgstr in ordered:
        translation_table.extend(struct.pack("<II", len(msgstr), cursor))
        cursor += len(msgstr) + 1

    header = struct.pack(
        "<7I",
        0x950412DE,
        0,
        count,
        originals_offset,
        translations_offset,
        0,
        0,
    )
    return header + bytes(original_table) + bytes(translation_table) + original_blob + translation_blob


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--po", type=Path, default=DEFAULT_PO)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--coverage", type=Path, default=DEFAULT_COVERAGE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--check", action="store_true", help="fail unless the checked-in MO is current")
    args = parser.parse_args()

    try:
        catalog = validate_catalog(args.po, args.source, args.coverage)
        compiled = compile_mo(catalog)
        if args.check:
            if not args.output.is_file() or args.output.read_bytes() != compiled:
                raise CatalogError(f"compiled catalog is missing or stale: {args.output}")
        else:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_bytes(compiled)
    except (CatalogError, OSError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(
        f"Validated {len(catalog) - 1} translations; "
        f"{'checked' if args.check else 'wrote'} {args.output} ({len(compiled)} bytes)."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
