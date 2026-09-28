#!/usr/bin/env python3
"""Shared PO parsing and deterministic GNU MO writing for the fork's catalogs.

Both fork-owned catalogs are compiled with this module: the Hong Kong Cantonese
catalog (bbl/i18n/yue_HK/compile_translation.py) and the English override
catalog (bbl/i18n/compile_catalog.py). Entries carry their message context and
plural forms, so the MO keys match what wxMsgCatalog looks up:

* a context entry is stored under ``ctx + "\\x04" + msgid``;
* a plural entry stores ``msgid + "\\0" + msgid_plural`` and its forms joined
  with ``"\\0"``.

The writer emits no hash table and sorts entries by their UTF-8 key, so the same
PO always produces the same bytes.
"""

from __future__ import annotations

import ast
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path
import re
import struct
from typing import Dict, Iterable, List, Optional, Tuple

PLACEHOLDER_RE = re.compile(
    r"%\d+%"
    r"|%%"
    r"|%(?:\d+\$)?[-+#0 'I]*(?:\d+|\*)?(?:\.(?:\d+|\*))?"
    r"(?:hh|h|ll|l|j|z|t|L)?[diuoxXfFeEgGaAcspn]"
    r"|\{\{\s*[^{}]+\s*\}\}"
    r"|\{[A-Za-z_][^{}]*\}"
)

REVIEW_STATUS_VALUES = ("agent-drafted",)


class CatalogError(ValueError):
    pass


@dataclass
class Entry:
    msgid: str = ""
    msgctxt: Optional[str] = None
    msgid_plural: Optional[str] = None
    msgstr: str = ""
    msgstr_plural: Dict[int, str] = field(default_factory=dict)
    categories: List[str] = field(default_factory=list)
    review_status: Optional[str] = None
    source_pending: Optional[str] = None
    flags: List[str] = field(default_factory=list)
    line: int = 0

    @property
    def key(self) -> Tuple[Optional[str], str]:
        return (self.msgctxt, self.msgid)

    @property
    def is_header(self) -> bool:
        return self.msgid == "" and self.msgctxt is None

    @property
    def is_plural(self) -> bool:
        return self.msgid_plural is not None

    @property
    def fuzzy(self) -> bool:
        return "fuzzy" in self.flags

    def forms(self) -> List[str]:
        if not self.is_plural:
            return [self.msgstr]
        return [self.msgstr_plural[i] for i in sorted(self.msgstr_plural)]

    def translated(self) -> bool:
        forms = self.forms()
        return bool(forms) and all(form.strip() for form in forms)

    def describe(self) -> str:
        return repr(self.msgid) if self.msgctxt is None else f"{self.msgid!r} (context {self.msgctxt!r})"


def _quoted(value: str, path: Path, line_number: int) -> str:
    try:
        parsed = ast.literal_eval(value)
    except (SyntaxError, ValueError) as exc:
        raise CatalogError(f"{path}:{line_number}: invalid PO string: {exc}") from exc
    if not isinstance(parsed, str):
        raise CatalogError(f"{path}:{line_number}: PO value is not a string")
    return parsed


_FIELD_RE = re.compile(r"(msgctxt|msgid_plural|msgid|msgstr(?:\[(\d+)\])?)\s+(\".*\")$")


def parse_po(path: Path) -> List[Entry]:
    """Parse a PO file. Obsolete (``#~``) entries are skipped entirely."""
    entries: List[Entry] = []
    current = Entry()
    started = False
    active: Optional[Tuple[str, Optional[int]]] = None

    def finish() -> None:
        nonlocal current, started, active
        if started:
            entries.append(current)
        current = Entry()
        started = False
        active = None

    lines = path.read_text(encoding="utf-8-sig").splitlines()
    for line_number, line in enumerate([*lines, ""], 1):
        stripped = line.strip()
        if not stripped:
            finish()
            continue
        if line.startswith("#~"):
            continue
        if line.startswith("#. reviewed-category: "):
            current.categories.append(line[len("#. reviewed-category: "):].strip())
            continue
        if line.startswith("#. review-status: "):
            current.review_status = line[len("#. review-status: "):].strip()
            continue
        if line.startswith("#. source-pending: "):
            current.source_pending = line[len("#. source-pending: "):].strip()
            continue
        if line.startswith("#,"):
            current.flags.extend(flag.strip() for flag in line[2:].split(",") if flag.strip())
            continue
        if line.startswith("#"):
            continue

        match = _FIELD_RE.match(line)
        if match:
            name, index, value = match.group(1), match.group(2), match.group(3)
            if name in ("msgctxt", "msgid") and started and active and active[0].startswith("msgstr"):
                # A new entry began without a blank separator line.
                finish()
            if not started:
                current.line = line_number
            started = True
            text = _quoted(value, path, line_number)
            if name == "msgctxt":
                current.msgctxt = text
                active = ("msgctxt", None)
            elif name == "msgid":
                current.msgid = text
                active = ("msgid", None)
            elif name == "msgid_plural":
                current.msgid_plural = text
                active = ("msgid_plural", None)
            elif index is not None:
                current.msgstr_plural[int(index)] = text
                active = ("msgstr_plural", int(index))
            else:
                current.msgstr = text
                active = ("msgstr", None)
            continue
        if line.startswith('"') and active:
            text = _quoted(line, path, line_number)
            kind, index = active
            if kind == "msgctxt":
                current.msgctxt = (current.msgctxt or "") + text
            elif kind == "msgid":
                current.msgid += text
            elif kind == "msgid_plural":
                current.msgid_plural = (current.msgid_plural or "") + text
            elif kind == "msgstr_plural":
                current.msgstr_plural[index] = current.msgstr_plural.get(index, "") + text
            else:
                current.msgstr += text
            continue
        raise CatalogError(f"{path}:{line_number}: unsupported PO syntax: {line}")

    return entries


def entry_map(entries: Iterable[Entry], path: Path, *, allow_duplicates: bool = False) -> Dict[Tuple[Optional[str], str], Entry]:
    result: Dict[Tuple[Optional[str], str], Entry] = {}
    for entry in entries:
        if entry.key in result:
            if allow_duplicates:
                continue
            raise CatalogError(f"{path}: duplicate msgid: {entry.describe()}")
        result[entry.key] = entry
    return result


def placeholders(value: str) -> List[str]:
    """Format placeholders in order of appearance.

    A space-flag match glued to a following letter ("94% if", "100% done")
    is ordinary prose, not a placeholder, and is skipped: counting it once
    pushed a translation to replace real numbers with fake "% i" tokens. So is
    one that follows a digit ("10% to 90%" reads "% to" as a length modifier
    and an octal conversion), which once left "% to" in a translation.
    """
    found = []
    for match in PLACEHOLDER_RE.finditer(value):
        token = match.group(0)
        end = match.end()
        if token.startswith("% ") and end < len(value) and value[end].isascii() and value[end].isalpha():
            continue
        if token.startswith("% ") and match.start() > 0 and value[match.start() - 1].isdigit():
            continue
        found.append(token)
    return found


def placeholder_signature(value: str) -> Counter:
    return Counter(placeholders(value))


_POSITIONAL = re.compile(r"^%\d+(\$|%)")


def sequential_placeholders(value: str) -> List[str]:
    """Placeholders bound by their order in the string (printf %s/%d, {}), excluding %1$s, %1% and %%.

    wxString::Format and boost::format fill these in order, so a translation that
    moves one relative to another swaps the facts it prints.
    """
    return [token for token in placeholders(value) if token != "%%" and not _POSITIONAL.match(token)]


def mo_original(entry: Entry) -> str:
    text = entry.msgid
    if entry.msgctxt is not None:
        text = entry.msgctxt + "\x04" + text
    if entry.is_plural:
        text = text + "\x00" + (entry.msgid_plural or "")
    return text


def mo_translation(entry: Entry) -> str:
    return "\x00".join(entry.forms())


def compile_mo(pairs: Iterable[Tuple[str, str]]) -> bytes:
    """Write a GNU MO file without a hash table, sorted by the UTF-8 original."""
    ordered: List[Tuple[bytes, bytes]] = sorted(
        ((original.encode("utf-8"), translation.encode("utf-8")) for original, translation in pairs),
        key=lambda item: item[0],
    )
    count = len(ordered)
    originals_offset = 7 * 4
    translations_offset = originals_offset + count * 8
    strings_offset = translations_offset + count * 8

    original_blob = b"".join(original + b"\0" for original, _ in ordered)
    translation_blob = b"".join(translation + b"\0" for _, translation in ordered)

    original_table = bytearray()
    translation_table = bytearray()
    cursor = strings_offset
    for original, _ in ordered:
        original_table.extend(struct.pack("<II", len(original), cursor))
        cursor += len(original) + 1
    cursor = strings_offset + len(original_blob)
    for _, translation in ordered:
        translation_table.extend(struct.pack("<II", len(translation), cursor))
        cursor += len(translation) + 1

    header = struct.pack("<7I", 0x950412DE, 0, count, originals_offset, translations_offset, 0, 0)
    return header + bytes(original_table) + bytes(translation_table) + original_blob + translation_blob


def read_mo(data: bytes) -> Dict[str, str]:
    """Decode an MO produced by compile_mo (little-endian, UTF-8) for round-trip checks."""
    magic, _revision, count, originals, translations, _hash_size, _hash_offset = struct.unpack_from("<7I", data, 0)
    if magic != 0x950412DE:
        raise CatalogError("not a little-endian GNU MO file")
    result: Dict[str, str] = {}
    for index in range(count):
        o_len, o_off = struct.unpack_from("<II", data, originals + index * 8)
        t_len, t_off = struct.unpack_from("<II", data, translations + index * 8)
        result[data[o_off:o_off + o_len].decode("utf-8")] = data[t_off:t_off + t_len].decode("utf-8")
    return result
