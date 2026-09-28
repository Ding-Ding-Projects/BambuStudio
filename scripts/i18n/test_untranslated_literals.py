#!/usr/bin/env python3
"""Guard against user-visible text that bypasses the translation layer.

    py -3 scripts/i18n/test_untranslated_literals.py

Scans every ``src/slic3r/GUI/**/*.cpp`` and ``**/*.hpp`` file for a
high-confidence pattern: a raw string literal (or a ``wxT("...")`` /
``wxString("...")`` literal) that looks like real prose -- it starts with a
letter and either contains a space or is a single capitalised word -- passed
directly as an argument to one of the call sites in ``TARGET_CALLS`` below
(``new wxStaticText(``, ``SetLabel(``, ``wxLogError(``, ``MessageDialog(``,
and so on). Such a literal can never be picked up by the catalogue: it never
passes through ``_L``/``_u8L``/``_(`` (see ``src/slic3r/GUI/I18N.hpp``), so it
can never appear translated, no matter what language mode is active.

A literal already wrapped in one of the real translation macros
(``_L``, ``_``, ``_u8L``, ``_CTX``, ``_CTX_utf8``, ``_utf8``, ``L``, ``_devL``,
``_omitL``, ``_CHB``, ``L_CONTEXT``, ``_L_PLURAL``) is not a bypass and is
skipped. ``wxT(...)`` and ``wxString(...)`` are NOT translation macros --
wrapping a literal in either of them still bypasses the catalogue, so both
forms are treated exactly like a bare literal.

Genuine exceptions (a brand name, a language self-name, a debug-only window,
and so on -- see the "leave alone" list in the project's localisation task
notes) are recorded in ``scripts/i18n/untranslated-literals.allow.json`` as
``{"file": "<path relative to the repo root, forward slashes>", "literal":
"<exact text between the quotes>", "reason": "<why this one is fine>"}``.
Every allowlist entry must carry a non-empty reason; the script refuses to
run against a malformed allowlist rather than silently ignoring it.

Exit status: 0 when every hit is allowlisted (or there are no hits), 1 when
at least one unallowlisted hit remains. Only the standard library is used.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path
from typing import Iterable, NamedTuple

REPO_ROOT = Path(__file__).resolve().parents[2]
SCAN_ROOT = REPO_ROOT / "src" / "slic3r" / "GUI"
ALLOWLIST_PATH = Path(__file__).resolve().with_name("untranslated-literals.allow.json")

# Real translation macros/functions (src/slic3r/GUI/I18N.hpp). A literal whose
# immediately preceding call is one of these is already extracted and
# translated; it is never a bypass.
SAFE_WRAPPERS = {
    "_L", "_", "_u8L", "_CTX", "_CTX_utf8", "_utf8", "L", "_devL", "_omitL",
    "_CHB", "L_CONTEXT", "_L_PLURAL", "wxGetTranslation",
}

# Call sites whose text argument reaches the user. The text can be any
# argument in the call (e.g. wxStaticText's label is its 3rd constructor
# argument), so each match bounds the whole argument list by matching parens
# and searches every literal inside it.
TARGET_CALLS = [
    r"new\s+wxStaticText\s*\(",
    r"new\s+Label\s*\(",
    r"new\s+Button\s*\(",
    r"new\s+wxButton\s*\(",
    r"SetLabel\s*\(",
    r"SetLabelText\s*\(",
    r"SetTitle\s*\(",
    r"SetToolTip\s*\(",
    r"SetHint\s*\(",
    r"ImGui::Text\s*\(",
    r"imgui\.text\s*\(",
    r"m_imgui->text\s*\(",
    r"MessageDialog\s*\(",
    r"MsgDialog\s*\(",
    r"wxMessageBox\s*\(",
    r"WarningDialog\s*\(",
    r"ErrorDialog\s*\(",
    r"wxLogError\s*\(",
    r"wxLogWarning\s*\(",
]
CALL_SITE_RE = re.compile("|".join(f"(?:{p})" for p in TARGET_CALLS))

# A "real prose" literal: starts with a letter, and either contains a space
# or (with no space at all) is a single word starting with a capital letter.
def looks_like_prose(text: str) -> bool:
    if not text or not text[0].isalpha():
        return False
    if " " in text:
        return True
    return text[0].isupper()


STRING_LITERAL_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')
PRECEDING_CALL_RE = re.compile(r"([A-Za-z_][A-Za-z0-9_:]*)\s*\($")


class Hit(NamedTuple):
    file: str  # repo-relative, forward slashes
    line: int
    literal: str
    context: str  # the matched call-site keyword, for a readable report


def strip_comments(text: str) -> str:
    """Blank out // and /* */ comments, preserving offsets and line breaks."""
    out = []
    i = 0
    n = len(text)
    in_string = False
    in_char = False
    while i < n:
        c = text[i]
        if in_string:
            out.append(c)
            if c == "\\" and i + 1 < n:
                out.append(text[i + 1])
                i += 2
                continue
            if c == '"':
                in_string = False
            i += 1
            continue
        if in_char:
            out.append(c)
            if c == "\\" and i + 1 < n:
                out.append(text[i + 1])
                i += 2
                continue
            if c == "'":
                in_char = False
            i += 1
            continue
        if c == '"':
            in_string = True
            out.append(c)
            i += 1
            continue
        if c == "'":
            in_char = True
            out.append(c)
            i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                out.append(" ")
                i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            out.append("  ")
            i += 2
            while i < n and not (text[i] == "*" and i + 1 < n and text[i + 1] == "/"):
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            if i < n:
                out.append("  ")
                i += 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


def find_matching_paren(text: str, open_index: int) -> int:
    """Return the index of the ')' matching the '(' at open_index.

    Skips over string and char literals so a stray ')' or '(' inside one
    cannot confuse the depth count. Returns -1 if unmatched (malformed/huge
    call the scanner should not guess about).
    """
    assert text[open_index] == "("
    depth = 0
    i = open_index
    n = len(text)
    in_string = False
    in_char = False
    while i < n:
        c = text[i]
        if in_string:
            if c == "\\" and i + 1 < n:
                i += 2
                continue
            if c == '"':
                in_string = False
            i += 1
            continue
        if in_char:
            if c == "\\" and i + 1 < n:
                i += 2
                continue
            if c == "'":
                in_char = False
            i += 1
            continue
        if c == '"':
            in_string = True
        elif c == "'":
            in_char = True
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def scan_file(path: Path) -> list[Hit]:
    raw = path.read_text(encoding="utf-8", errors="replace")
    clean = strip_comments(raw)
    rel = path.relative_to(REPO_ROOT).as_posix()
    hits: list[Hit] = []

    for call_match in CALL_SITE_RE.finditer(clean):
        open_paren = clean.find("(", call_match.start())
        if open_paren == -1:
            continue
        close_paren = find_matching_paren(clean, open_paren)
        if close_paren == -1:
            continue
        args = clean[open_paren + 1:close_paren]
        args_offset = open_paren + 1

        # C++ concatenates adjacent string literals ("a" "b" == "ab"), a
        # common way to spell a long message across several lines. Group
        # consecutive matches separated only by whitespace into one chain, so
        # only the first segment's wrapper matters: _L("a" "b") is one
        # translated literal, not one translated segment plus a bypassing one.
        matches = list(STRING_LITERAL_RE.finditer(args))
        chains: list[list[re.Match]] = []
        for lit_match in matches:
            if chains and args[chains[-1][-1].end():lit_match.start()].strip() == "":
                chains[-1].append(lit_match)
            else:
                chains.append([lit_match])

        for chain in chains:
            head = chain[0]
            before = args[:head.start()]
            wrapper = PRECEDING_CALL_RE.search(before)
            if wrapper and wrapper.group(1) in SAFE_WRAPPERS:
                continue  # the whole concatenated literal is already translated

            for lit_match in chain:
                literal_text = lit_match.group(1)
                if not looks_like_prose(literal_text):
                    continue
                abs_offset = args_offset + lit_match.start()
                line_no = clean.count("\n", 0, abs_offset) + 1
                hits.append(Hit(rel, line_no, literal_text, call_match.group(0).strip()))
                break  # one hit per concatenated literal is enough to act on

    return hits


def load_allowlist() -> set[tuple[str, str]]:
    if not ALLOWLIST_PATH.exists():
        return set()
    data = json.loads(ALLOWLIST_PATH.read_text(encoding="utf-8"))
    if not isinstance(data, list):
        raise SystemExit(f"{ALLOWLIST_PATH}: expected a JSON array of entries")

    allowed: set[tuple[str, str]] = set()
    for index, entry in enumerate(data):
        if not isinstance(entry, dict):
            raise SystemExit(f"{ALLOWLIST_PATH}: entry {index} is not an object")
        file = entry.get("file")
        literal = entry.get("literal")
        reason = entry.get("reason")
        if not isinstance(file, str) or not file:
            raise SystemExit(f"{ALLOWLIST_PATH}: entry {index} is missing a non-empty \"file\"")
        if not isinstance(literal, str) or not literal:
            raise SystemExit(f"{ALLOWLIST_PATH}: entry {index} is missing a non-empty \"literal\"")
        if not isinstance(reason, str) or not reason.strip():
            raise SystemExit(
                f"{ALLOWLIST_PATH}: entry {index} ({file!r}, {literal!r}) has no non-empty \"reason\""
            )
        allowed.add((file, literal))
    return allowed


def iter_source_files() -> Iterable[Path]:
    if not SCAN_ROOT.is_dir():
        raise SystemExit(f"Scan root not found: {SCAN_ROOT}")
    yield from sorted(SCAN_ROOT.rglob("*.cpp"))
    yield from sorted(SCAN_ROOT.rglob("*.hpp"))


def main() -> int:
    allowed = load_allowlist()

    all_hits: list[Hit] = []
    for path in iter_source_files():
        all_hits.extend(scan_file(path))

    unallowed = [h for h in all_hits if (h.file, h.literal) not in allowed]

    if not unallowed:
        if all_hits:
            print(f"test_untranslated_literals: OK - {len(all_hits)} literal(s) found, all allowlisted.")
        else:
            print("test_untranslated_literals: OK - no untranslated bypass literals found.")
        return 0

    print(f"test_untranslated_literals: FAILED - {len(unallowed)} untranslated literal(s):\n")
    for hit in sorted(unallowed, key=lambda h: (h.file, h.line)):
        print(f"  {hit.file}:{hit.line}: {hit.context} \"{hit.literal}\"")
    print(
        "\nWrap each literal in _L()/_u8L() (or _L_PLURAL/_CTX as appropriate), or add a "
        f"genuine exception to {ALLOWLIST_PATH.relative_to(REPO_ROOT).as_posix()} with a reason."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
