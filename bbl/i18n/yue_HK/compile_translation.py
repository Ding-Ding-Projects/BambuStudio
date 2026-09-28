#!/usr/bin/env python3
"""Validate the yue_HK catalog and compile a deterministic GNU MO.

Checks, in order:

* the header declares ``Language: yue_HK`` (and ``Plural-Forms`` when the
  catalog has plural entries);
* every entry is translated, keeps its placeholders, and carries exactly one
  ``#. reviewed-category:`` comment (plus an optional
  ``#. review-status: agent-drafted`` marker);
* every entry exists in the English source catalog, unless
  ``--allow-unreferenced`` is given or the entry carries
  ``#. source-pending: <where the source lives>`` (a translation written ahead
  of code that has not reached this branch yet);
* ``coverage.json`` counts match the catalog;
* with ``--require-complete``, every English source message has a Cantonese
  entry (``--report-missing`` writes the gap list as JSON).

Context (``msgctxt``) and plural entries are compiled with the keys wxWidgets
looks up; see ``bbl/i18n/po_catalog.py``.
"""

from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path
import sys
from typing import Dict, List, Optional, Tuple

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR.parent))

from po_catalog import (  # noqa: E402  (path set up above)
    REVIEW_STATUS_VALUES,
    CatalogError,
    Entry,
    compile_mo,
    entry_map,
    mo_original,
    mo_translation,
    parse_po,
    placeholder_signature,
    read_mo,
    sequential_placeholders,
)

REPO_ROOT = SCRIPT_DIR.parents[2]
DEFAULT_PO = SCRIPT_DIR / "BambuStudio_yue_HK.po"
DEFAULT_SOURCE = SCRIPT_DIR.parent / "en" / "BambuStudio_en.po"
DEFAULT_COVERAGE = SCRIPT_DIR / "coverage.json"
DEFAULT_OUTPUT = REPO_ROOT / "resources" / "i18n" / "yue_HK" / "BambuStudio.mo"
MINIMUM_TRANSLATIONS = 150

Key = Tuple[Optional[str], str]


def english_display(entry: Entry) -> str:
    """The English the UI shows for a source entry: its override, else the msgid."""
    if entry.is_plural:
        forms = entry.forms()
        return forms[-1] if forms and forms[-1].strip() else (entry.msgid_plural or entry.msgid)
    return entry.msgstr if entry.msgstr.strip() else entry.msgid


def validate_catalog(
    po_path: Path,
    source_path: Path,
    coverage_path: Path,
    *,
    require_source_membership: bool = True,
    require_complete: bool = False,
    report_missing: Optional[Path] = None,
) -> List[Tuple[str, str]]:
    target_entries = parse_po(po_path)
    target = entry_map(target_entries, po_path)
    source: Dict[Key, Entry] = {}
    if require_source_membership or require_complete or report_missing:
        # The upstream English extraction can repeat a msgid; the repeats are
        # the same lookup key, so collapse them for membership checks.
        source = entry_map(parse_po(source_path), source_path, allow_duplicates=True)

    header = target.get((None, ""))
    if header is None or "Language: yue_HK\n" not in header.msgstr:
        raise CatalogError("catalog header must declare Language: yue_HK")
    has_plural = any(entry.is_plural for entry in target.values())
    if has_plural and "Plural-Forms: nplurals=1; plural=0;" not in header.msgstr:
        raise CatalogError("catalog header must declare 'Plural-Forms: nplurals=1; plural=0;' for plural entries")

    pairs: List[Tuple[str, str]] = [("", header.msgstr)]
    category_counts: Counter = Counter()
    drafted = 0
    for key, entry in target.items():
        if entry.is_header:
            continue
        if entry.fuzzy:
            raise CatalogError(f"fuzzy entries are not allowed: {entry.describe()}")
        if not entry.translated():
            raise CatalogError(f"empty translation: {entry.describe()}")
        if entry.is_plural and len(entry.forms()) != 1:
            raise CatalogError(f"Cantonese has one plural form; found {len(entry.forms())}: {entry.describe()}")
        if require_source_membership and key not in source and not entry.source_pending:
            raise CatalogError(f"msgid is absent from English source catalog: {entry.describe()}")
        reference = entry.msgid_plural if entry.is_plural else entry.msgid
        for form in entry.forms():
            if placeholder_signature(reference or "") != placeholder_signature(form):
                raise CatalogError(
                    f"placeholder mismatch for {entry.describe()}: "
                    f"{placeholder_signature(reference or '')} != {placeholder_signature(form)}"
                )
            if sequential_placeholders(reference or "") != sequential_placeholders(form):
                raise CatalogError(
                    f"unnumbered placeholders reordered for {entry.describe()}: "
                    f"{sequential_placeholders(reference or '')} != {sequential_placeholders(form)}"
                )
        if len(entry.categories) != 1:
            raise CatalogError(f"exactly one reviewed-category is required: {entry.describe()}")
        if entry.review_status is not None and entry.review_status not in REVIEW_STATUS_VALUES:
            raise CatalogError(f"unknown review-status {entry.review_status!r}: {entry.describe()}")
        if entry.review_status == "agent-drafted":
            drafted += 1
        category_counts[entry.categories[0]] += 1
        pairs.append((mo_original(entry), mo_translation(entry)))

    translated = len(pairs) - 1
    coverage = json.loads(coverage_path.read_text(encoding="utf-8"))
    if coverage.get("translated_messages") != translated:
        raise CatalogError(
            f"coverage.json translated_messages is {coverage.get('translated_messages')}, catalog has {translated}"
        )
    if coverage.get("categories") != dict(sorted(category_counts.items())):
        raise CatalogError("coverage.json category counts do not match reviewed-category comments")
    if coverage.get("agent_drafted_messages", 0) != drafted:
        raise CatalogError(
            f"coverage.json agent_drafted_messages is {coverage.get('agent_drafted_messages', 0)}, catalog has {drafted}"
        )
    if translated < MINIMUM_TRANSLATIONS:
        raise CatalogError(f"at least {MINIMUM_TRANSLATIONS} reviewed native translations are required")

    if require_complete or report_missing:
        missing = [entry for key, entry in source.items() if not entry.is_header and key not in target]
        if report_missing:
            report_missing.parent.mkdir(parents=True, exist_ok=True)
            report_missing.write_text(
                json.dumps(
                    [
                        {
                            "msgctxt": entry.msgctxt,
                            "msgid": entry.msgid,
                            "msgid_plural": entry.msgid_plural,
                            "english": english_display(entry),
                        }
                        for entry in missing
                    ],
                    ensure_ascii=False,
                    indent=1,
                ),
                encoding="utf-8",
            )
        if require_complete and missing:
            sample = ", ".join(entry.describe() for entry in missing[:5])
            raise CatalogError(f"{len(missing)} English source messages have no Cantonese entry (first: {sample})")

    return pairs


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--po", type=Path, default=DEFAULT_PO)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--coverage", type=Path, default=DEFAULT_COVERAGE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument(
        "--check",
        action="store_true",
        help="write nothing; verify the compiled catalog round-trips and, when --output exists, is byte-identical",
    )
    parser.add_argument(
        "--allow-unreferenced",
        action="store_true",
        help="allow Cantonese keys not yet present in the English extraction",
    )
    parser.add_argument(
        "--require-complete",
        action="store_true",
        help="fail unless every English source message has a Cantonese entry",
    )
    parser.add_argument("--report-missing", type=Path, help="write the untranslated English source messages as JSON")
    args = parser.parse_args()

    try:
        pairs = validate_catalog(
            args.po,
            args.source,
            args.coverage,
            require_source_membership=not args.allow_unreferenced,
            require_complete=args.require_complete,
            report_missing=args.report_missing,
        )
        compiled = compile_mo(pairs)
        decoded = read_mo(compiled)
        for original, translation in pairs:
            if decoded.get(original) != translation:
                raise CatalogError(f"compiled catalog does not round-trip: {original!r}")
        if args.check:
            if args.output.is_file() and args.output.read_bytes() != compiled:
                raise CatalogError(f"compiled catalog is stale: {args.output}")
        else:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_bytes(compiled)
    except (CatalogError, OSError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    action = "checked" if args.check else "wrote"
    print(f"Validated {len(pairs) - 1} translations; {action} {args.output} ({len(compiled)} bytes).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
