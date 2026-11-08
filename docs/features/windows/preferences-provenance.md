# Preferences: where every value comes from

**Surface:** Preferences (`Ctrl+P`, or Settings in the navigation rail) →
`PreferencesDialog` (`src/slic3r/GUI/Preferences.{hpp,cpp}`), with the change
record and the provenance rules in
`src/slic3r/GUI/PreferencesRemake/PreferenceHistory.{hpp,cpp}` and the plans
for section reset, import, undo and related settings in
`src/slic3r/GUI/PreferencesRemake/PreferencePlans.{hpp,cpp}`.

Design: [design/local-tools-2026-11](../../../design/local-tools-2026-11/README.md),
board InfoIcons.

## On every setting row

Every row with a title column now has two more things (the Appearance rows,
Theme, Density, Accent color, Font and Text size, included):

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
- **Playground**, for the Appearance settings: a sample button, switch,
  progress bar and line of text, built again after every change so they wear
  the look in effect. They change nothing in your project;
- **Related**: up to five other settings under the same heading (the rest of
  the section fills in when the heading holds fewer than two others); each
  chip goes to that setting and opens its explanation;
- **Reset to** the built-in default, for switches, choices and the Appearance
  controls: it works the row's own control, so every side effect the control
  normally triggers still runs. A row whose control is disabled right now
  (School mode, an element lock, a gate) is never changed from here, and rows
  without such a control say to change them back with their own control.

Values are named the way the row shows them: "Dark", "Compact", "Large",
"Green" rather than `1`, `compact`, `1.15` or `#146c2e`.

## Section counts

Each section in the strip shows how many settings it holds, for example
"Appearance (5)". The sections keep their ids and names, which saved strip
layouts and command palette links depend on.

## Reset this section

**Reset this section**, beside the search field, plans the section shown:

- every setting that differs from its built-in default and has a control that
  can take it is listed with the value it goes back to;
- settings that restart the app, sign you out or answer a consent question
  (language, single instance, level of detail, region, the experience
  program, the developer modes) are left for their own controls, and so are
  text fields and sliders;
- private values (tokens, codes, keys) are never reset from here.

Nothing changes until both keys are turned and the slide is completed (the
two-key confirmation). Each setting then changes through its own control, and
the history records what it was. Anything that could not take its default,
and anything left for its own control, is listed afterwards.

## Import

**Import preferences...** reads a file written by **Export preferences...**
as JSON (with or without the schema header, at most 1 MB) and lists what
would change, up to 12 settings and how many more. Only settings Preferences
shows are considered; private values, settings Preferences does not show and
values that are not text, true or false, or a whole number are counted and
skipped, and the settings that are left for their own controls (as above) are
named. A file from a newer version, or one that is not a preferences export,
is refused with the reason. After you agree, each setting changes through its
own control; a control that does not offer the value, or is locked, keeps
what it had and is named afterwards.

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

**Undo last change** puts back the newest change only, through its row's own
control, when that row has one and is not locked; an empty earlier value means
nothing was stored then, so the built-in default goes back. The undo is
recorded like any other change, so pressing it again redoes. A private value,
or a setting left for its own control, says so instead.

Export preferences... still exports every section in any of its formats.

## Checks

`tests/preference_plans.test.mjs` builds `tests/preference_plans_test.cpp`:
the section reset plan (reset, left for its own control, unchanged, private,
a key that appears twice), import (the envelope and the bare data, every
refusal, values that are not text, private and unknown keys), undo (newest
only, defaults for an empty earlier value, every reason), related settings
and section counts. Its negative build also resets the settings that must be
changed by hand, and must fail.

`tests/preferences_tools_wiring.test.mjs` checks the dialog source: Reset this
section changes nothing before the two-key confirmation and only what the plan
lists, import changes nothing before you agree to the preview, every change
goes through the row's own control and never a disabled one, the consent row
has no setter, and the panel's Reset and Related chips act after their click
returns; each checker must fail on a broken copy.

`tests/preference_history.test.mjs` builds `tests/preference_history_test.cpp`
(redaction, the record format, bad and CRLF lines, compaction, every
provenance case) and a negative build in which nothing counts as private,
which must fail. It also checks the dialog source: rows are decorated when they
register, provenance lines stay out of search, tracking starts after the search
index and the timer stops with the dialog.

## Not done yet

The board's twelve regrouped sections are not adopted (the sections keep the
ids saved layouts and palette links use). There is no Needs a restart chip or
Restart now, text fields and sliders have no reset of their own, and import
reads JSON only.
