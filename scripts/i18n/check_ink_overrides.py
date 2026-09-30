#!/usr/bin/env python3
"""Every message that names the printing material or the dispenser must show the ink wording.

The rename is display only: a msgid keeps the upstream words, and the English override catalogue
(`bbl/i18n/en/BambuStudio_en.po`) and the Cantonese catalogue carry what a person reads. A check of
the translated values alone passes on a message that has no override at all, and such a message is
shown as its msgid, with the old words. This check starts from the other end: the extracted
template lists every message the application can show, and each one that uses an old word must
have an English override that does not.

It reads whole entries, so a multi-line msgid or msgstr is checked too.

    py -3 scripts/i18n/check_ink_overrides.py            # exit 1 and a list when something is missing
    py -3 scripts/i18n/check_ink_overrides.py --count    # print only the number of problems
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "bbl" / "i18n"))
import po_catalog  # noqa: E402

OLD_MATERIAL = re.compile(r"\bfilaments?\b", re.IGNORECASE)
OLD_DISPENSER = re.compile(r"\bAMS\b")
# In a Cantonese value a Latin word sits directly against Chinese characters ("AMS槽位"), and a
# Chinese character counts as a word character, so "\b" never fires there. These patterns look
# only at the Latin letters, digits and underscore around the word. The Chinese words are the
# older names for the material and for changing it; 切換線框 (toggle wireframe) is not one of them.
OLD_CANTONESE = re.compile(
    r"(?<![A-Za-z0-9_])AMS(?![A-Za-z0-9_])"
    r"|(?<![A-Za-z0-9_])[Ff]ilaments?(?![A-Za-z0-9_])|FILAMENT"
    r"|線材|耗材|絲材|墨水絲|(?<!切)換線(?!框)|換料"
)

# Messages that name the old words on purpose. Each needs a reason.
KEPT_AS_WRITTEN = {
    # The title of the article that explains the rename; it has to name both sides of it.
    "Ink terminology (filament → ink, AMS → Ink Dispenser)",
}


def uses_old_word(text: str) -> bool:
    return bool(OLD_MATERIAL.search(text) or OLD_DISPENSER.search(text))


def shown_forms(entry: po_catalog.Entry) -> list[str]:
    return [form for form in entry.forms() if form.strip()]


def problems() -> list[str]:
    template = po_catalog.parse_po(REPO / "bbl" / "i18n" / "BambuStudio.pot")
    english_path = REPO / "bbl" / "i18n" / "en" / "BambuStudio_en.po"
    cantonese_path = REPO / "bbl" / "i18n" / "yue_HK" / "BambuStudio_yue_HK.po"
    english = {entry.key: entry for entry in po_catalog.parse_po(english_path) if not entry.is_header}
    found: list[str] = []

    # 1. Every extracted message that uses an old word has a clean English override.
    required = 0
    for entry in template:
        if entry.is_header or entry.msgid in KEPT_AS_WRITTEN:
            continue
        sources = [entry.msgid] + ([entry.msgid_plural] if entry.msgid_plural else [])
        if not any(uses_old_word(source) for source in sources):
            continue
        required += 1
        override = english.get(entry.key)
        if override is None or not override.translated():
            found.append(f"[en] no override, shown with the old words: {entry.describe()}")

    # 2. No English or Cantonese value uses an old word, whatever its msgid says.
    for label, path, pattern in (
        ("en", english_path, None),
        ("yue_HK", cantonese_path, OLD_CANTONESE),
    ):
        for entry in po_catalog.parse_po(path):
            if entry.is_header or entry.msgid in KEPT_AS_WRITTEN:
                continue
            for form in shown_forms(entry):
                bad = uses_old_word(form) if pattern is None else bool(pattern.search(form))
                if bad:
                    found.append(f"[{label}] {path.name}:{entry.line} still says the old word: {form[:110]!r}")
                    break

    print(f"ink overrides: {required} extracted messages use an old word; {len(found)} problem(s).")
    return found


def main() -> int:
    found = problems()
    if "--count" not in sys.argv:
        for line in found:
            print("  " + line)
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
