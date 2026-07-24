# Command palette (Ctrl+F)

**Surface:** `Ctrl+F` anywhere in the main frame → `CommandPalette`
(`src/slic3r/GUI/CommandPalette.{hpp,cpp}`).

## Behavior

- One searchable surface over everything: every enabled menubar command
  (path-titled, e.g. "File / Import / Import 3MF…", executed by posting its
  `wxEVT_MENU` id), the main navigation targets (Home / Prepare / Preview /
  Device / Project), feature landmarks (Preferences, parameter search), and
  quick-settings rows.
- Every row carries a Material Symbols **icon**, a **title**, and a
  **description** (the menu item's help string for menu commands).
- **Rich rows**: entries that are settings render live controls inline
  instead of a bare action — theme Light/Dark and density
  Comfortable/Compact as segmented switches, accent color as six seed
  swatches. Changes apply through the same config keys and MD3 pipelines as
  Preferences ▸ Appearance.
- The query field is the shared MD3 `SearchField`: plain text by default,
  `.*` regex toggle, and the full regex builder popover — the palette obeys
  the same search rules as every other search bar.
- Keyboard: type to filter, Up/Down select, Enter runs, Esc closes. Rows are
  capped at 120 per query to keep the palette instant; refining the query
  reveals the rest.

## Failure modes / notes

- Disabled menu items are excluded at open time, so the palette can never
  run a command its menu would refuse.
- Menu commands run *after* the palette closes (posted events), so modal
  follow-ups (file dialogs) never fight the palette for focus.

## Verification

- Driven headlessly (Mesa llvmpipe + Lowlevel MCP): Ctrl+F accelerator opens
  the palette, filtering narrows rows, Enter on "Go to Prepare" switches
  tabs, the theme row's segmented switch re-themes the live UI.
