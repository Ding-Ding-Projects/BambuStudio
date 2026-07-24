# Regex builder

Every search bar in the app is a shared MD3 `SearchField` widget with direct
access to a full guided regex builder (`RegexBuilderPopup`): sectioned token
construction, a raw pattern editor, flag toggles, live syntax feedback,
bounded sample-text testing with match and capture-group listing, and
copy/export — all evaluated against the app's real regex engine.

## Engine, dialect, flags, escaping

- **Engine**: `std::regex` / `std::wregex` with the **ECMAScript** grammar —
  the exact engine and dialect used by every in-app search
  (`SearchField::textMatches`, `Search.cpp`, `ImGuiWrapper.cpp`). The builder
  never previews with a different dialect than the search executes, and the
  popover itself carries an engine caption stating this.
- **Case sensitivity**: the *Case sensitive* flag maps to
  `std::regex_constants::icase` (added when case sensitivity is off).
- **Whole word**: applies to plain-text (non-regex) search only; in regex mode
  authors use `\b` themselves. The builder says so next to the flag.
- **Escaping**: a backslash escapes the ECMAScript metacharacters
  `\ ^ $ . | ? * + ( ) [ ] { }`. The *Literals* section performs this escaping
  automatically.

## Reaching the builder

- Each `SearchField` shows a persistent trailing `.*` **regex toggle** (plain
  text stays the default; regex mode is a deliberate opt-in that also switches
  the entry to Roboto Mono) and a `tune` **builder button** that opens the
  popover.
- Search surfaces hosting the shared field include: parameter search
  (`Tab.cpp` / `Search.cpp`), the Plater object search (`Plater.cpp`),
  Preferences (`Preferences.cpp`), user presets (`UserPresetsDialog.cpp`),
  device selection (`SelectMachinePop.cpp`), and the multi-machine manager
  (`MultiMachineManagerPage.cpp`). Every one of them gets the identical
  builder from the widget — no surface carries a reduced variant.
- Query, pattern, flags, and mode stay synchronized bidirectionally: typing in
  the field updates the popover's raw editor live, and edits in the popover
  re-fire the field's query callback so the host list re-filters immediately.

## Popover anatomy (`src/slic3r/GUI/Widgets/RegexBuilderPopup.{hpp,cpp}`)

1. **Title + engine caption** — names `std::regex` / `std::wregex`, the
   ECMAScript grammar, the `icase` flag, and the backslash escaping rule.
2. **Pattern** — raw pattern editor (Roboto Mono), bidirectionally synced with
   the owning field's query, plus a copy-to-clipboard icon button
   (`wxClipboard`).
3. **Validity line** — live feedback on every keystroke: Primary-coloured
   "Valid pattern" (with a match count when sample text is present) or an
   Error-coloured message mapping the `std::regex_error` code to a friendly
   description (unbalanced `[ ]` / `( )` / `{ }`, bad counts, invalid escape,
   invalid backreference, invalid range, dangling quantifier, unknown class,
   too-complex pattern).
4. **Flags** — *Regex mode* (the `.*` toggle), *Case sensitive*, and *Whole
   word* checkboxes. Flag changes re-fire the field's regex-toggle callback so
   consumers re-run their filter with the shared `textMatches()` semantics.
5. **Guided sections** — labelled chip palettes, each chip with an explanatory
   tooltip; clicking (or Enter/Space) inserts the token at the pattern caret,
   and `()` / `[]` style tokens land the caret inside the pair:
   - *Literals*: a text input plus **Add** that inserts the text with all
     metacharacters escaped;
   - *Character classes*: `.` `[ ]` `[^ ]` `a-z` `\d \D \w \W \s \S`;
   - *Anchors*: `^` `$` `\b` `\B`;
   - *Groups & alternation*: `( )` capturing, `(?: )` non-capturing, `|`,
     `\1` backreference;
   - *Quantifiers*: `*` `+` `?` `{n}` `{n,}` `{n,m}` and the lazy variants
     `*?` `+?` `??`.
6. **Test pattern** (collapsible, collapsed by default — progressive
   disclosure keeps the popover compact) — a bounded multiline sample-text
   input with live match highlighting (SecondaryContainer tonal spans) and a
   results list showing each match's ordinal, character range, matched text,
   and every capture group (`group N: text` / `group N: (no match)`).

The popover hosts real child controls inside a transient popup using the
`wxPU_CONTAINS_CONTROLS` pattern proven by `Search.cpp`'s `SearchDialog`, so
text fields receive focus without breaking outside-click dismissal; `Esc`
dismisses it explicitly. Content lives in a vertical `wxScrolledWindow` whose
height is capped to the display (fits an 800px-tall screen at 100–200% scale;
overflow scrolls). The popover is rebuilt on every open so colours, fonts and
`FromDIP` metrics re-derive per open — theme, DPI, and density safe. All
colours resolve through `StateColor::semantic(MD3::Role::...)`; there are no
hardcoded light-mode literals, and every control carries an accessible name
and is keyboard reachable (chip palettes are one tab stop each, with
arrow-key navigation and a Primary focus ring).

## Matching semantics (`SearchField::textMatches`)

- empty query → matches everything;
- plain text → substring test honouring *Case sensitive*; *Whole word*
  constrains hits to `\b`-style boundaries without regex parsing;
- regex → `std::wregex` search with `icase` when case-insensitive; an invalid
  or half-typed pattern **matches all** (`true`) so a list is never blanked
  mid-edit.

## Configuration

Nothing is persisted beyond the per-field mode toggle; patterns, flags, and
sample text are session state. `SearchField::GetPattern()` exposes the
current pattern to hosts.

## Security considerations and bounds

- Patterns and sample text are evaluated **locally only** — never
  transmitted, logged, or persisted.
- **Pattern cap**: 2000 characters (enforced by the editor's max length and
  re-checked before compilation for programmatic syncs).
- **Sample cap**: 20000 characters (editor max length; longer programmatic
  content is truncated before evaluation, with a notice in the results).
- **Match cap**: the first 200 matches are listed/highlighted, with a notice.
- **Catastrophic backtracking / regex DoS**: `std::regex` has no timeout, so
  the guard is the small bounded inputs plus `try`/`catch` around every
  compile and search — `error_complexity`, `error_space`, and `error_stack`
  surface as "Pattern too complex to evaluate safely" instead of hanging or
  throwing out of the popover. Match sites in the app (`textMatches`,
  `Search.cpp`, `ImGuiWrapper.cpp`) wrap their own `std::regex_search` in
  `try`/`catch` as well.
- Zero-width matches are advanced safely (`std::regex_iterator` semantics) and
  never painted as highlights.

## Failure modes

- **Invalid pattern** → red friendly message in the builder's validity line;
  the field itself degrades to match-all so the host list stays populated.
- **Pathological pattern** → bounded, guarded evaluation; the builder reports
  "Pattern too complex to evaluate safely" and the search degrades to
  no-filter rather than hanging or crashing.
- **Oversized inputs** → hard caps with explicit truncation notices.
- **Clipboard busy** → copy is skipped silently (no data loss; pattern stays
  in the editor).

## Verification

- Register row `no-shared-md3-searchfield` (design-system parity register):
  done.
- Search fields and the builder popover captured per button in the screenshot
  matrix under `docs/screenshots/regex-builder/`.
- Manual matrix for the builder: valid / invalid / no-match patterns, Unicode
  sample text, multiline samples, zero-width matches (`a*`), capture groups,
  adversarial nested-quantifier patterns, and plain-text-versus-regex flag
  behaviour — exercised against the app's real `std::wregex` engine from the
  parameter-search and Plater search surfaces.
