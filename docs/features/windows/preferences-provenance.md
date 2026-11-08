# Preferences: where every value comes from

**Surface:** Preferences (`Ctrl+P`, or Settings in the navigation rail) →
`PreferencesDialog` (`src/slic3r/GUI/Preferences.{hpp,cpp}`), with the change
record and the provenance rules in
`src/slic3r/GUI/PreferencesRemake/PreferenceHistory.{hpp,cpp}`.

Design: [design/local-tools-2026-11](../../../design/local-tools-2026-11/README.md),
board InfoIcons.

## On every setting row

Every row with a title column now has two more things:

- **A line under its title that says where the value comes from.** One of:
  - "Built-in default: On. You have not changed it."
  - "You set Dark on 2026-11-02 at 18:40:11." (from the change record)
  - "Stored as 110. The built-in default is 100." (changed before the record
    existed, or by something other than this dialog)
  - "Scheduled settings set this to Off for now." (a schedule is overriding it)
  - "This value is private, so it is not shown here." (tokens, access codes,
    station keys and similar)
- **An info button** that opens the explanation panel on the right.

The built-in default is the value `AppConfig::set_defaults()` declares, read
from a configuration built only from those defaults, so it is never guessed.
A setting without a declared default says so instead of inventing one.

## The explanation panel

- what the setting does (the row's own description);
- where the value comes from, in the words above;
- the built-in default;
- when it takes effect: right away, after a restart, or the next time the
  feature that uses it runs;
- where it is stored (`BambuStudio.conf` and its key);
- the last five changes to it, with their times;
- **Reset**, for switches whose built-in default is known: it flips the row's
  own switch, so every side effect the switch normally triggers still runs.
  Other rows say to change them back with their own control.

## Changed by you

The chip beside the search field shows how many settings differ from their
built-in default, and turning it on hides every other row. It works together
with the search text, plain or regular expression. Opening a setting from the
command palette or the Every feature page turns the chip off so the row shows.

## Preferences history

The Preferences history button lists every change made while Preferences was
open, newest first, with its time and old and new value. The record is a
JSON-lines file, `preferences-history.jsonl`, in the data folder. It never
leaves the computer. Private keys are recorded only as "changed", without
either value. The file keeps the newest 1,000 changes.

Export preferences... still exports every section in any of its formats.

## Checks

`tests/preference_history.test.mjs` builds `tests/preference_history_test.cpp`
(redaction, the record format, bad and CRLF lines, compaction, every
provenance case) and a negative build in which nothing counts as private,
which must fail. It also checks the dialog source: rows are decorated when they
register, provenance lines stay out of search, tracking starts after the search
index and the timer stops with the dialog.

## Not done yet

The board's regrouped sections with counts, Reset this section behind two-key
confirmation, related settings, the try-it playground and import are designed
but not built.
