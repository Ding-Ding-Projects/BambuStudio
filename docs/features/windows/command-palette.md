# Command palette (Ctrl+Shift+F)

**Surface:** `Ctrl+Shift+F` anywhere in the main frame → `CommandPalette`
(`src/slic3r/GUI/CommandPalette.{hpp,cpp}`), backed by the pure-data index in
`src/slic3r/GUI/CommandPaletteIndex.{hpp,cpp}`.

## Activation

- `Ctrl+Shift+F` is the one global shortcut. It is listed under *Global
  shortcuts* in the keyboard-shortcuts dialog (`KBShortcutsDialog`). `Ctrl+F`
  is no longer bound to the palette (and is not bound to anything else at the
  frame level).
- The main frame installs **one** accelerator table built from
  `PaletteIndex::main_frame_accelerators()`: the six `Ctrl+Numpad1..6` tab
  chords (select workspace tab N-1 when that page exists) plus
  `Ctrl+Shift+F`. `wxWindow::SetAcceleratorTable()` replaces rather than
  merges, so the palette chord used to be installed in its own one-entry table
  and silently wiped the numpad entries; both families now live in the same
  list, and `tests/command_palette` asserts they coexist.
- Opening the palette while it is already open is a no-op (one instance).

## Size choice

- A toggle button beside the search pill switches between the **bounded
  card** (default, 640 × ~500 dip, centred over the frame) and the **full
  window** (covers the main frame's client area). The button is a real MD3
  IconButton with an accessible name/tooltip describing the state it switches
  to ("Expand the palette to the full window" / "Shrink the palette to a
  card").
- The choice persists in `AppConfig` under `palette_size` (`card` | `full`);
  unknown or missing values mean card. `PaletteIndex::load_palette_size` /
  `store_palette_size` are store-agnostic so the round trip is unit-tested
  without an AppConfig.

## What is indexed

Every row carries a Material Symbols **icon**, a **title**, and a
**description**.

| Source | Rows | Runs |
| --- | --- | --- |
| Quick settings | Theme, Density, Accent color | rich inline controls (segmented switch / seed swatches) |
| Workspace tabs | every `MainFrame::TabPosition` the frame actually built a page for (Home, Prepare, Preview, Device, Multi-device, Project, Calibration, Filament) | `select_tab()` |
| Landmarks | Open Preferences, Search in settings | opens Preferences / the sidebar parameter search |
| Preferences settings | every setting the Preferences dialog renders, titled `Preferences / <page> / <label>` | **teleport** (below) |
| Menubar | every enabled menu item, path-titled (`File / Import / …`) | posts the item's `wxEVT_MENU` id |
| Documentation | every article under `docs/features/**` (title only, `Documentation / <title>`) | opens the rendered article in the browser (`https://github.com/Ding-Ding-Projects/BambuStudio/blob/main/<path>` — the Pages site does not host the articles as pages yet) |

Disabled menu items are excluded at open time, so the palette can never run a
command its menu would refuse. Rows on the developer page only appear in
non-public builds, matching the dialog.

## Teleport

Selecting a Preferences row does not merely open Preferences:
`GUI_App::open_preferences(key)` opens the dialog and, once its modal loop is
running, calls `PreferencesDialog::teleport_to_setting(key)`, which

1. clears any live search filter (a filter would hide the row),
2. selects the owning page in the nav rail and the book,
3. scrolls the row into view (`scroll_search_row_into_view`),
4. focuses the row's first focusable control (switch, combo, input, button),
5. tints the row's labels with the Primary role for ~1.4 s and restores them.

Rows are located by AppConfig key: every `create_item_*` builder registers
the key of the row it returns (`register_option_row`), and the Appearance
rows register `dark_color_mode`, `ui_density`, `ui_accent_seed`,
`ui_font_family` and `ui_font_scale`. `build_search_index()` folds that
registry into `SearchRow::keys`. If a key is not registered (a gated row in
this build), the dialog falls back to selecting the page recorded in the
index.

## Search

The query field is the shared MD3 `SearchField`: plain text by default, `.*`
regex toggle, and the full regex builder popover — the palette obeys the same
search rules as every other search bar. Keyboard: type to filter, Up/Down
select, Enter runs, Esc closes. Rows are capped at 120 per query to keep the
palette instant; refining the query reveals the rest.

## Completeness guard

`tests/command_palette/command_palette_tests_main.cpp` (Catch2, links only
`CommandPaletteIndex.cpp` + wx base/core) holds:

- a **hand-written list** of settings, workspace tabs, Preferences pages and
  articles that MUST be reachable — a rule-only test passes on an empty
  index, so the list is the point;
- a **source-scan guard**: every AppConfig key that `Preferences.cpp` binds a
  row to (the key argument of each `create_item_*` call, named constants
  resolved from the GUI headers, plus explicit `register_option_row("…")`
  registrations) must appear in `preference_entries()`, and vice versa;
- a **directory-scan guard**: every `docs/features/**/*.md` (README indexes
  excluded) must appear in `documentation_articles()` with its current H1;
- the accelerator-table test (numpad entries and `Ctrl+Shift+F` together,
  no `Ctrl+F`, unique ids), the size-choice round trip, and teleport target
  resolution for known keys (`use_inches` → General, `dark_color_mode` →
  Appearance, `backup_interval` → Other).

Adding a preference or an article without updating the index fails the
build's test step rather than shipping a palette that quietly lacks it.

## Failure modes / notes

- Menu commands run *after* the palette closes (posted events), so modal
  follow-ups (file dialogs) never fight the palette for focus.
- A full-window palette is sized from the frame's client rectangle at open
  time; it does not follow a live frame resize while open.
- The article links point at the repository's rendered Markdown, not the
  Pages site, until the site publishes the articles.

## Verification

- `command_palette_tests` (Catch2) covers the accelerator table, persistence,
  teleport resolution and the three completeness guards.
- Headless drive (Mesa llvmpipe + Lowlevel MCP): `Ctrl+Shift+F` opens the
  palette, filtering narrows rows, Enter on "Go to Prepare" switches tabs,
  the theme row's segmented switch re-themes the live UI. The `palette`
  command of the layout probe (`LayoutProbe.cpp`) opens it without the chord.
