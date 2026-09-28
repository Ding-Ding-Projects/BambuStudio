#!/usr/bin/env python3
"""Check that the Hong Kong Cantonese documentation and changelog keep up with the English.

    py -3 scripts/i18n/check_translated_content.py

Documentation: every article under docs/features (category README.md indexes
excepted) has a Cantonese sibling <name>.yue_HK.md. The sibling starts with front
matter whose ``source-sha256`` is the SHA-256 of the English article's text with
CRLF line endings normalised to LF, so a checkout's line-ending setting never
decides the verdict. A missing sibling or a changed English article fails the
check: the translation has to follow the article it translates.

Changelog: every entry in resources/changelog/changelog.json has a non-empty
Cantonese text in resources/changelog/changelog.yue_HK.json. Translations for
entries that no longer exist are reported but do not fail.

Prints one line per problem and a summary; exits 1 when anything fails.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re
import sys

REPO_ROOT = Path(__file__).resolve().parents[2]
DOCS = REPO_ROOT / "docs" / "features"
CHANGELOG = REPO_ROOT / "resources" / "changelog" / "changelog.json"
CHANGELOG_YUE = REPO_ROOT / "resources" / "changelog" / "changelog.yue_HK.json"
TRANSLATION_SUFFIX = ".yue_HK.md"
FRONT_MATTER = re.compile(r"\A---\r?\n(.*?)\r?\n---\r?\n", re.S)


def english_hash(path: Path) -> str:
    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def check_docs(failures: list) -> int:
    articles = [p for p in sorted(DOCS.rglob("*.md"))
                if p.name != "README.md" and not p.name.endswith(TRANSLATION_SUFFIX)]
    for article in articles:
        sibling = article.with_name(article.name[: -len(".md")] + TRANSLATION_SUFFIX)
        rel = article.relative_to(REPO_ROOT).as_posix()
        if not sibling.is_file():
            failures.append(f"{rel}: no Cantonese translation ({sibling.name})")
            continue
        match = FRONT_MATTER.match(sibling.read_text(encoding="utf-8"))
        meta = {}
        if match:
            for line in match.group(1).splitlines():
                key, _, value = line.partition(":")
                meta[key.strip()] = value.strip()
        recorded = meta.get("source-sha256", "")
        if not recorded:
            failures.append(f"{rel}: {sibling.name} has no source-sha256 front matter")
        elif recorded != english_hash(article):
            failures.append(f"{rel}: changed since {sibling.name} was translated")
    orphans = [p for p in sorted(DOCS.rglob("*" + TRANSLATION_SUFFIX))
               if not p.with_name(p.name[: -len(TRANSLATION_SUFFIX)] + ".md").is_file()]
    for orphan in orphans:
        failures.append(f"{orphan.relative_to(REPO_ROOT).as_posix()}: translation without an English article")
    return len(articles)


def check_changelog(failures: list, notes: list) -> int:
    document = json.loads(CHANGELOG.read_text(encoding="utf-8"))
    shas = {entry["sha"] for release in document.get("releases", []) for entry in release.get("entries", [])}
    if not CHANGELOG_YUE.is_file():
        failures.append(f"{CHANGELOG_YUE.relative_to(REPO_ROOT).as_posix()}: missing")
        return len(shas)
    translations = json.loads(CHANGELOG_YUE.read_text(encoding="utf-8")).get("entries", {})
    missing = sorted(sha for sha in shas if not str(translations.get(sha, "")).strip())
    for sha in missing:
        failures.append(f"changelog entry {sha[:9]}: no Cantonese text")
    stale = sorted(set(translations) - shas)
    if stale:
        notes.append(f"{len(stale)} Cantonese changelog entries name commits the changelog no longer lists")
    return len(shas)


def main() -> int:
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    failures: list = []
    notes: list = []
    articles = check_docs(failures)
    entries = check_changelog(failures, notes)
    for line in failures:
        print(line)
    for line in notes:
        print(f"note: {line}")
    print(f"Checked {articles} articles and {entries} changelog entries: {len(failures)} problem(s).")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
