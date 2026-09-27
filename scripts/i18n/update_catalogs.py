#!/usr/bin/env python3
"""Regenerate the source catalog and bring the English override catalog up to date.

    py -3 scripts/i18n/update_catalogs.py [--gettext-bin DIR] [--located-pot PATH] [--report-missing PATH]

Steps:

1. Find GNU gettext (``--gettext-bin``, ``BAMBU_GETTEXT_BIN``, a previous
   bootstrap under ``%LOCALAPPDATA%\\gettext-iconv``, or ``PATH``). When none is
   found, download the pinned portable release from its canonical GitHub
   release, verify its SHA-256, and unpack it user-scoped.
2. Run xgettext over ``bbl/i18n/list.txt`` with the keywords in
   ``bbl/i18n/xgettext-keywords.txt`` (the same flags as the CMake
   ``gettext_make_pot`` target) into ``bbl/i18n/BambuStudio.pot``, then append
   the ``[hint:*]`` texts from ``resources/data/*hints.ini`` exactly as the
   ``hintsToPot`` tool does.
3. Append every new message to ``bbl/i18n/en/BambuStudio_en.po`` with an empty
   msgstr. Nothing is removed or marked obsolete: an entry the extractor no
   longer sees may still be looked up at run time with a dynamic key, and its
   English override must survive.
4. Optionally write a located copy of the extraction (for translators) and the
   list of messages that still lack a Cantonese entry.

Only standard-library Python is used.
"""

from __future__ import annotations

import argparse
import configparser
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

REPO_ROOT = Path(__file__).resolve().parents[2]
I18N_DIR = REPO_ROOT / "bbl" / "i18n"
sys.path.insert(0, str(I18N_DIR))

from po_catalog import Entry, entry_map, parse_po  # noqa: E402

GETTEXT_RELEASE = "v1.0-v1.19"
GETTEXT_ASSET = "gettext1.0-iconv1.19-shared-64.zip"
GETTEXT_SHA256 = "c2f195fc4ed3df4070fb08ff88c2f724afd1eed4e28f859efae1000bb7ba1152"
GETTEXT_URL = f"https://github.com/mlocati/gettext-iconv-windows/releases/download/{GETTEXT_RELEASE}/{GETTEXT_ASSET}"
HINT_FILES = ("hints.ini", "helio_hints.ini")


def log(message: str) -> None:
    print(f"[catalogs] {message}", flush=True)


def find_gettext(explicit: str | None) -> Path:
    candidates = []
    if explicit:
        candidates.append(Path(explicit))
    if os.environ.get("BAMBU_GETTEXT_BIN"):
        candidates.append(Path(os.environ["BAMBU_GETTEXT_BIN"]))
    local = os.environ.get("LOCALAPPDATA")
    if local:
        candidates.append(Path(local) / "gettext-iconv" / GETTEXT_RELEASE / "bin")
    on_path = shutil.which("xgettext")
    if on_path:
        candidates.append(Path(on_path).parent)
    for directory in candidates:
        if (directory / "xgettext.exe").is_file() or (directory / "xgettext").is_file():
            return directory
    if not local:
        raise SystemExit("GNU gettext is missing and LOCALAPPDATA is unset; pass --gettext-bin.")
    return bootstrap_gettext(Path(local) / "gettext-iconv" / GETTEXT_RELEASE)


def bootstrap_gettext(destination: Path) -> Path:
    log(f"GNU gettext not found; downloading {GETTEXT_ASSET} from {GETTEXT_URL}")
    destination.mkdir(parents=True, exist_ok=True)
    archive = destination / GETTEXT_ASSET
    with urllib.request.urlopen(GETTEXT_URL, timeout=120) as response, open(archive, "wb") as out:
        shutil.copyfileobj(response, out)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if digest != GETTEXT_SHA256:
        archive.unlink(missing_ok=True)
        raise SystemExit(f"{GETTEXT_ASSET} SHA-256 {digest} does not match the pinned {GETTEXT_SHA256}")
    with zipfile.ZipFile(archive) as bundle:
        bundle.extractall(destination)
    log(f"Installed GNU gettext user-scoped at {destination}")
    return destination / "bin"


def read_keywords() -> list[str]:
    keywords = []
    for line in (I18N_DIR / "xgettext-keywords.txt").read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            keywords.append(line)
    return keywords


def run_xgettext(gettext_bin: Path, output: Path, *, locations: bool) -> None:
    command = [
        str(gettext_bin / "xgettext"),
        "--no-wrap",
        "--add-comments=TRN",
        "--from-code=UTF-8",
        "--debug",
        "--boost",
        *[f"--keyword={keyword}" for keyword in read_keywords()],
        "-f", str(I18N_DIR / "list.txt"),
        "-o", str(output),
    ]
    if not locations:
        command.insert(1, "--no-location")
    result = subprocess.run(command, cwd=REPO_ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        raise SystemExit(f"xgettext failed with exit code {result.returncode}")
    warnings = [line for line in result.stderr.splitlines() if "warning" in line.lower()]
    if warnings:
        log(f"xgettext reported {len(warnings)} warnings (first: {warnings[0]})")


def append_hints(pot: Path) -> int:
    """Mirror src/hints/HintsToPot.cpp: one entry per [hint:*] section's text key, written raw."""
    added = 0
    with pot.open("a", encoding="utf-8", newline="\n") as out:
        for name in HINT_FILES:
            ini_path = REPO_ROOT / "resources" / "data" / name
            parser = configparser.RawConfigParser(strict=False, interpolation=None, comment_prefixes=(";", "#"))
            parser.optionxform = str
            parser.read(ini_path, encoding="utf-8")
            for section in parser.sections():
                if section.startswith("hint:") and parser.has_option(section, "text"):
                    out.write(f"\n#: resources/data/{name}: [{section}]\nmsgid \"{parser.get(section, 'text')}\"\nmsgstr \"\"\n")
                    added += 1
    return added


def po_quote(text: str) -> str:
    escaped = (
        text.replace("\\", "\\\\").replace('"', '\\"').replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n")
    )
    return f'"{escaped}"'


def entry_block(entry: Entry) -> list[str]:
    lines = []
    if entry.flags:
        lines.append("#, " + ", ".join(entry.flags))
    if entry.msgctxt is not None:
        lines.append(f"msgctxt {po_quote(entry.msgctxt)}")
    lines.append(f"msgid {po_quote(entry.msgid)}")
    if entry.is_plural:
        lines.append(f"msgid_plural {po_quote(entry.msgid_plural or '')}")
        lines.append('msgstr[0] ""')
        lines.append('msgstr[1] ""')
    else:
        lines.append('msgstr ""')
    return lines


def merge_into_english(pot: Path, english_po: Path) -> tuple[int, int]:
    pot_entries = entry_map(parse_po(pot), pot, allow_duplicates=True)
    english_entries = entry_map(parse_po(english_po), english_po, allow_duplicates=True)
    new = [entry for key, entry in pot_entries.items() if not entry.is_header and key not in english_entries]
    stale = [key for key, entry in english_entries.items() if not entry.is_header and key not in pot_entries]
    if new:
        raw = english_po.read_bytes()
        newline = "\r\n" if b"\r\n" in raw else "\n"
        text = raw.decode("utf-8")
        blocks = [newline.join(entry_block(entry)) for entry in new]
        tail = "" if text.endswith(newline) else newline
        english_po.write_bytes((text + tail + newline + (newline + newline).join(blocks) + newline).encode("utf-8"))
    return len(new), len(stale)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--gettext-bin")
    parser.add_argument("--located-pot", type=Path, help="also write an extraction with source locations here")
    parser.add_argument("--report-missing", type=Path, help="write messages without a Cantonese entry as JSON")
    args = parser.parse_args()

    gettext_bin = find_gettext(args.gettext_bin)
    log(f"Using GNU gettext from {gettext_bin}")
    pot = I18N_DIR / "BambuStudio.pot"
    run_xgettext(gettext_bin, pot, locations=False)
    hints = append_hints(pot)
    pot_count = sum(1 for entry in parse_po(pot) if not entry.is_header)
    log(f"Extracted {pot_count} messages into {pot.relative_to(REPO_ROOT)} ({hints} from hints)")

    if args.located_pot:
        args.located_pot.parent.mkdir(parents=True, exist_ok=True)
        run_xgettext(gettext_bin, args.located_pot, locations=True)
        append_hints(args.located_pot)
        log(f"Wrote located extraction to {args.located_pot}")

    english_po = I18N_DIR / "en" / "BambuStudio_en.po"
    added, stale = merge_into_english(pot, english_po)
    log(f"English catalog: {added} new messages appended; {stale} existing entries are no longer extracted (kept)")

    if args.report_missing:
        compiler = I18N_DIR / "yue_HK" / "compile_translation.py"
        with tempfile.TemporaryDirectory() as scratch:
            result = subprocess.run(
                [sys.executable, str(compiler), "--allow-unreferenced", "--output", str(Path(scratch) / "yue.mo"),
                 "--report-missing", str(args.report_missing)],
                cwd=REPO_ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace",
            )
        if result.returncode != 0:
            sys.stderr.write(result.stderr)
            raise SystemExit("compile_translation.py failed while reporting missing messages")
        log(f"Wrote the Cantonese gap list to {args.report_missing}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
