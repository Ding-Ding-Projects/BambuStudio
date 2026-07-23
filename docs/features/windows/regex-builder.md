# Regex builder

Every search bar in the app is a shared MD3 `SearchField` widget with direct
access to a full regex builder: a guided pattern-construction popover, raw
pattern editing, live validation, and safe evaluation against the app's real
regex engine.

## Behavior

- **Engine**: `std::regex` (ECMAScript dialect), the same engine used by every
  in-app search operation — the builder never previews with a different
  dialect than the search executes.
- **Search field** (`src/slic3r/GUI/Widgets/SearchField.{hpp,cpp}`, consumed by
  11 search surfaces incl. parameter search, object list, and preset lists):
  - plain-text search is the default; the `.*` toggle deliberately enables
    regex mode;
  - the tune button opens the builder popover;
  - query, pattern, flags, and mode stay synchronized bidirectionally.
- **Builder popover**: guided construction (literals, character classes,
  anchors, groups, alternation, quantifiers), raw pattern editor, flag
  toggles, sample text with live matches and capture groups, syntax feedback,
  and copy/export of the pattern.
- **Matching** (`SearchField::textMatches`): invalid patterns match-all
  (`true`) so a half-typed pattern never blanks a list; match sites
  (`Search.cpp`, `ImGuiWrapper.cpp`) wrap `std::regex_search` in try/catch as
  a catastrophic-backtracking/regex-error guard.

## Configuration

None persisted beyond the per-field mode toggle; patterns are session state.

## Failure modes

- **Invalid pattern** → inline syntax feedback in the builder; match-all in
  the field (list stays populated).
- **Pathological patterns** (catastrophic backtracking) → guarded evaluation;
  the search degrades to no-filter rather than hanging or crashing.

## Security considerations

Patterns and sample text are evaluated locally and never transmitted or
persisted. Pattern length and sample size are bounded by the UI controls;
evaluation exceptions are contained at every match site.

## Verification

- Register row `no-shared-md3-searchfield` (design-system parity register):
  done.
- Search fields and the builder popover captured per button in the screenshot
  matrix under `docs/screenshots/regex-builder/`.
