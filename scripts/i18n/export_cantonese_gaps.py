#!/usr/bin/env python3
"""Cut the untranslated English messages into translation batches.

    py -3 scripts/i18n/export_cantonese_gaps.py --missing missing.json --located-pot located.pot --out DIR [--batch-size 275]

Inputs are produced by the catalogue tools:

* ``--missing``: ``compile_translation.py --report-missing`` JSON (msgctxt, msgid,
  msgid_plural, english);
* ``--located-pot``: ``update_catalogs.py --located-pot`` output, used only for
  the ``#:`` source references so a batch keeps one screen's strings together.

Each batch file ``batch-NN.json`` lists entries with an ``id`` (stable within the
run), the English the UI shows, the formal Traditional Chinese reference from
``bbl/i18n/zh_TW`` when one exists (a hint, never to be copied as Cantonese),
the placeholders and line-break count a draft must keep, and the source files.
"""

from __future__ import annotations

import argparse
from collections import OrderedDict
import json
from pathlib import Path
import re
import sys

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "bbl" / "i18n"))

from po_catalog import PLACEHOLDER_RE, entry_map, parse_po  # noqa: E402


def located_sources(pot: Path) -> dict:
    """Map (msgctxt, msgid) -> [source files] from a located extraction."""
    sources: dict = {}
    refs: list = []
    ctx = None
    field = None
    msgid = None
    for line in pot.read_text(encoding="utf-8").splitlines() + [""]:
        if line.startswith("#: "):
            for ref in line[3:].split():
                refs.append(ref.rsplit(":", 1)[0] if re.search(r":\d+$", ref) else ref)
            continue
        if line.startswith("msgctxt "):
            ctx = json.loads(line[8:]) if line[8:].startswith('"') else None
            field = "ctx"
            continue
        if line.startswith("msgid "):
            msgid = json.loads(line[6:])
            field = "id"
            continue
        if line.startswith('"') and field in ("ctx", "id"):
            value = json.loads(line)
            if field == "ctx":
                ctx = (ctx or "") + value
            else:
                msgid = (msgid or "") + value
            continue
        if line.startswith("msgid_plural") or line.startswith("msgstr"):
            field = None
            continue
        if not line.strip():
            if msgid is not None:
                sources.setdefault((ctx, msgid), [])
                for ref in refs:
                    if ref not in sources[(ctx, msgid)]:
                        sources[(ctx, msgid)].append(ref)
            refs, ctx, field, msgid = [], None, None, None
    return sources


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--missing", type=Path, required=True)
    parser.add_argument("--located-pot", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--batch-size", type=int, default=275)
    args = parser.parse_args()

    missing = json.loads(args.missing.read_text(encoding="utf-8"))
    sources = located_sources(args.located_pot)
    zh_tw_path = REPO_ROOT / "bbl" / "i18n" / "zh_TW" / "BambuStudio_zh_TW.po"
    zh_tw = entry_map(parse_po(zh_tw_path), zh_tw_path, allow_duplicates=True) if zh_tw_path.is_file() else {}

    groups: "OrderedDict[str, list]" = OrderedDict()
    for index, item in enumerate(missing):
        key = (item.get("msgctxt"), item["msgid"])
        files = sources.get(key, [])
        reference = zh_tw.get(key)
        reference_text = None
        if reference is not None and reference.translated():
            reference_text = reference.forms()[-1]
        english = item["english"]
        entry = {
            "id": f"m{index:05d}",
            "msgctxt": item.get("msgctxt"),
            "msgid": item["msgid"],
            "msgid_plural": item.get("msgid_plural"),
            "english": english,
            "zh_TW_reference": reference_text,
            "placeholders": sorted(set(PLACEHOLDER_RE.findall(item.get("msgid_plural") or item["msgid"]))),
            "line_breaks": (item.get("msgid_plural") or item["msgid"]).count("\n"),
            "sources": files[:4],
        }
        group = files[0] if files else "(no source reference)"
        groups.setdefault(group, []).append(entry)

    args.out.mkdir(parents=True, exist_ok=True)
    batches: list = [[]]
    for group, entries in groups.items():
        if batches[-1] and len(batches[-1]) + len(entries) > args.batch_size:
            batches.append([])
        batches[-1].extend(entries)
        while len(batches[-1]) > args.batch_size:
            overflow = batches[-1][args.batch_size:]
            batches[-1] = batches[-1][: args.batch_size]
            batches.append(overflow)
    for number, batch in enumerate(batches, 1):
        (args.out / f"batch-{number:02d}.json").write_text(json.dumps(batch, ensure_ascii=False, indent=1), encoding="utf-8")
    print(f"Wrote {len(batches)} batches ({sum(len(b) for b in batches)} messages) to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
