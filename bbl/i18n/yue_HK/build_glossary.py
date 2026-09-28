#!/usr/bin/env python3
"""Build glossary.json, the term list every Cantonese translation must follow.

    py -3 bbl/i18n/yue_HK/build_glossary.py

The glossary is derived from the curated (not agent-drafted) catalog entries whose
English is a short term, plus the fixed product vocabulary below. Translators read
it before drafting, and scripts/i18n/merge_cantonese_drafts.py checks drafts
against the fixed terms.
"""

from __future__ import annotations

import json
from pathlib import Path
import sys

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR.parent))

from po_catalog import parse_po, placeholder_signature  # noqa: E402

# Fixed product vocabulary (docs/features/windows/ink-terminology.md and STYLE.md).
FIXED_TERMS = {
    "Filament": "墨水",
    "Ink": "墨水",
    "AMS": "墨水機",
    "Ink Dispenser": "墨水機",
    "Project": "項目",
    "Printer": "打印機",
    "Print": "打印",
    "Software": "軟件",
    "Network": "網絡",
    "File": "檔案",
    "Save": "儲存",
    "Delete": "刪除",
    "Remove": "移除",
    "Overwrite": "覆寫",
    "Cancel": "取消",
}

# Terms that belong to formal Taiwan written Chinese, Simplified Chinese, or the
# retired filament wording. A Cantonese draft must use the Hong Kong term instead.
DISALLOWED_TERMS = {
    "專案": "項目",
    "軟體": "軟件",
    "網路": "網絡",
    "印表機": "打印機",
    "列印": "打印",
    "線材": "墨水",
    "耗材": "墨水",
    "檔案夾": "資料夾",
}


def main() -> int:
    entries = parse_po(SCRIPT_DIR / "BambuStudio_yue_HK.po")
    derived = {}
    for entry in entries:
        if entry.is_header or entry.is_plural or entry.review_status == "agent-drafted":
            continue
        english = entry.msgid.strip()
        if not english or len(english.split()) > 3 or placeholder_signature(english) or "\n" in english:
            continue
        derived.setdefault(english, entry.msgstr.strip())
    glossary = {
        "fixed_terms": FIXED_TERMS,
        "disallowed_terms": DISALLOWED_TERMS,
        "curated_short_terms": dict(sorted(derived.items(), key=lambda item: item[0].lower())),
    }
    output = SCRIPT_DIR / "glossary.json"
    output.write_text(json.dumps(glossary, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    print(f"Wrote {output} ({len(derived)} curated short terms).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
