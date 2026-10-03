#!/usr/bin/env python3
"""Refresh derived Cantonese coverage counts without changing review metadata.

This is an explicit authoring command, never a build-time repair. The candidate
metadata must pass the same catalog validation as a normal build before it can
replace coverage.json. No PO or compiled MO file is written.
"""

from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path
import shutil
import sys
import tempfile

from compile_translation import (
    DEFAULT_COVERAGE,
    DEFAULT_PO,
    DEFAULT_SOURCE,
    validate_catalog,
)
from po_catalog import CatalogError, compile_mo, entry_map, parse_po, read_mo


def refresh_coverage(
    po_path: Path,
    source_path: Path,
    coverage_path: Path,
    *,
    require_source_membership: bool = True,
    require_complete: bool = False,
) -> bool:
    """Validate and atomically replace stale counts; return whether they changed."""
    coverage_path = coverage_path.resolve(strict=True)
    original = coverage_path.read_bytes()
    coverage = json.loads(original.decode("utf-8"))
    if not isinstance(coverage, dict):
        raise CatalogError("coverage.json must contain a JSON object")

    po_snapshot = po_path.read_bytes()
    entries = entry_map(parse_po(po_path), po_path)
    messages = [entry for entry in entries.values() if not entry.is_header]
    categories = Counter(category for entry in messages for category in entry.categories)
    updated = dict(coverage)
    updated.update(
        translated_messages=len(messages),
        categories=dict(sorted(categories.items())),
        agent_drafted_messages=sum(entry.review_status == "agent-drafted" for entry in messages),
    )

    # Stage beside the destination so replacement stays on the same filesystem.
    # Invalid catalog data must never overwrite the last usable metadata.
    with tempfile.TemporaryDirectory(prefix=".coverage-", dir=coverage_path.parent) as directory:
        candidate = Path(directory) / "coverage.json"
        candidate.write_text(json.dumps(updated, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        pairs = validate_catalog(
            po_path,
            source_path,
            candidate,
            require_source_membership=require_source_membership,
            require_complete=require_complete,
        )
        decoded = read_mo(compile_mo(pairs))
        if any(decoded.get(original_key) != translation for original_key, translation in pairs):
            raise CatalogError("compiled catalog does not round-trip")
        if po_path.read_bytes() != po_snapshot:
            raise CatalogError("catalog changed during coverage refresh; retry with stable inputs")
        if coverage_path.read_bytes() != original:
            raise CatalogError("coverage.json changed during refresh; refusing to overwrite it")
        if updated == coverage:
            return False
        shutil.copymode(coverage_path, candidate)
        candidate.replace(coverage_path)
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--po", type=Path, default=DEFAULT_PO)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--coverage", type=Path, default=DEFAULT_COVERAGE)
    parser.add_argument("--allow-unreferenced", action="store_true",
                        help="allow keys not yet present in the English extraction, as in the build")
    parser.add_argument("--require-complete", action="store_true",
                        help="require a Cantonese entry for every English source message")
    args = parser.parse_args()
    try:
        changed = refresh_coverage(
            args.po,
            args.source,
            args.coverage,
            require_source_membership=not args.allow_unreferenced,
            require_complete=args.require_complete,
        )
    except (CatalogError, OSError, UnicodeError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    action = "Updated" if changed else "Already current:"
    print(f"{action} {args.coverage}; catalog validated, review metadata preserved.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
