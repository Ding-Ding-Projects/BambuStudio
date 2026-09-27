# Prepare sidebar search

Two shared MD3 `SearchField` pills in the Prepare left sidebar, complementing the existing
Objects search bar and the settings-tab magnifier. Both are 40 px stadium pills with the
kit anatomy: leading `search` glyph, `.*` regex-mode toggle, and the `tune` button opening
the full guided regex builder popover (raw pattern editor, token sections, flags,
sample-text tester). Implemented in `src/slic3r/GUI/Plater.cpp` (`Sidebar` /
`Sidebar::priv`), reusing `src/slic3r/GUI/Widgets/SearchField.{hpp,cpp}` and the global
option index in `src/slic3r/GUI/Search.{hpp,cpp}`.

## Settings search (Process card — covers Process, Printer and Filament options)

- A `SearchField` sits at the top of the compact Process card, under the "Process"
  section header (placeholder: *Search settings*).
- Focusing the field opens the global `Search::OptionsSearcher` results popup — the same
  `SearchDialog` the settings-tab magnifier uses — anchored directly under the pill.
  Typing filters live (fuzzy/substring by default, bounded Boost.Regex 1.84
  wide-character ECMAScript when regex mode is on;
  an invalid or half-typed pattern never hides results).
- Scope is `Preset::TYPE_INVALID`, i.e. **every preset type the searcher indexes for the
  current mode**: process/print, **printer** and filament options. This is why the
  Printer section carries no third search bar — its options are reachable from this one
  field.
- Activating a result (click, or Enter/arrow keys — the rows are keyboard-focusable)
  posts `wxCUSTOMEVT_JUMP_TO_OPTION`; `Sidebar::jump_to_option` then flips the sidebar
  to Advanced settings for print options (transiently, without persisting the user's
  compact/advanced choice) or activates the owning Printer/Filament tab option.
- The pill's `.*` toggle and the builder popover's case-sensitive / whole-word
  checkboxes are wired into the searcher's persistent flags by the `SearchDialog`.

## Filament slot search (Filament section)

- A compact `SearchField` sits inside the collapsible filament area wrapper, under the
  "Filament" section header and above the slot rows (placeholder: *Search filaments*).
- Filters the visible filament slot rows live as you type. The per-row haystack is
  `"<slot number> <preset name> #RRGGBB <nearest colour name>"`, using
  `SearchField::colorSearchText` — identical semantics to the Objects search — so
  `PETG`, `2`, `#00AE42` or `green` all match.
- Matching is `SearchField::textMatches`: substring by default, regex via the `.*`
  toggle, honouring the builder's case-sensitive / whole-word flags. An empty query
  shows every row; a non-matching row is `Hide()`-den and the physical scroll area is
  re-measured (`Sidebar::recalc_filament_scroll_sizes`, which re-applies the filter on
  every add/remove/rescale path so newly added or renamed slots stay filtered
  correctly).
- The filter also re-evaluates when a slot's preset or badge changes
  (`update_filament_row_badges`), so renames and colour edits update the result set.

## Regex builder access

Both pills bundle the mandatory regex builder: click the `tune` icon in the trailing
area of either field to open the guided builder popover (literals, character classes,
anchors, groups, alternation, quantifiers, flags, live match/capture testing, copy).
The `.*` pill toggles regex mode; plain-text search is the default.

## Layout, theming, DPI

- Both fields live inside their section's collapsible panel (the Process card / the
  filament area wrapper), so collapsing the section hides them.
- Colours and radii resolve live from MD3 tokens per paint — dark mode needs no extra
  wiring. `Sidebar::msw_rescale` calls `Rescale()` on both pills.

## Failure modes

- Invalid / half-typed regex: matches everything (nothing is hidden) in both surfaces.
- Catastrophic-backtracking patterns: caught (`std::regex_error`) and treated as
  match-all; the option searcher additionally bounds pattern length.
- All filament rows filtered out: the slot list collapses to empty; clearing the query
  (the pill's clear button) restores every row.

## Verification

1. Build and open Prepare with a multi-filament project.
2. Process card: click the *Search settings* pill, type `wall` — results list options
   across Process/Printer/Filament with type markers; click one and confirm the jump
   (print options flip the sidebar to Advanced settings; printer options open the
   Printer tab).
3. Type a printer-only option (e.g. `nozzle diameter`) and confirm the Printer tab jump.
4. Filament section: type a preset substring, a slot number, a colour name (`green`)
   and a hex value (`#RRGGBB` of a slot) — only matching rows stay; clear restores all.
5. Toggle `.*` and use a pattern (e.g. `PLA|PETG`); open the `tune` builder and verify
   flags re-run both filters live.
6. Collapse each section — the pills hide with their section. Change DPI/theme — pills
   re-derive geometry and colours.
7. Catalog: from `bbl/i18n/yue_HK`, `compile_translation.py` and
   `compile_translation.py --check` both pass (620 messages).
