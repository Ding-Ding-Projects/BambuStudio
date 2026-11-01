# Graphite shell

The Graphite shell is a redesign of the main window chrome. It changes where the workspaces live and the neutral palette every Material Design 3 surface paints from. Slicing, printing, presets, projects and every feature of this fork are unchanged.

## What changed

- **Navigation rail.** The main workspace book (`MainFrame::m_tabpanel`) docks left with `wxNB_LEFT`. `ButtonsListCtrl` lays its destinations out as an 88 px rail: each destination is a 72 x 60 px tile with the Material Symbols glyph above the label, the selected tile takes the secondary container, and a short primary bar on the leading edge is the non-colour cue. Navigation actions (Settings) are pinned to the bottom. When the window is too short, the overflow menu takes the tail exactly as the strip did, measured along the height with the same `NotebookNavigation::fit()`.
- **Every other notebook keeps the top strip.** A notebook created without a dock style still defaults to `wxBK_TOP`, so the settings dialog and its sub-tabs are untouched.
- **Graphite palette.** The neutral ramp in `MD3Tokens.hpp` becomes a green-tinted graphite. Dark: ground `#0e1311`, containers `#0a0e0c` to `#28322c`, text `#e3ebe5` and `#a9b6ad`, primary `#6fdc98`. Light: ground `#f5f8f5`, containers `#ffffff` to `#d6e1da`, text `#131c17` and `#3f4c44`, primary `#146c2e`. Text keeps at least 4.5:1 and outlines at least 3:1 against every surface in both themes, and the dark compatibility map stays idempotent (`tests/native_navigation/neutral_palette.test.mjs`).
- **New tokens.** `navigation_rail_width`, `navigation_rail_item_width`, `navigation_rail_item_height` and `radius_panel` (24 px, for floating panels and the canvas frame).

## Verification

- `g++ -fsyntax-only` against wxWidgets 3.2, Boost and OpenCASCADE headers on Linux passes for `Notebook.cpp` and `MainFrame.cpp`.
- `tests/native_navigation/graphite_rail.test.mjs` pins the left dock, the top-strip default for other notebooks, the rail geometry and the Graphite grounds.
- The node contract suite reports the same failures as `main` and no new ones. The fingerprint of `MainFrame::init_tabpanel` in `tests/native_shared_controls/atlas_shell_preserved_functions.json` was refreshed because this change deliberately edits that function.

Not verified yet: the hosted Windows build, the running application, captures in English, Cantonese and bilingual modes at each scale, and the keyboard order through the rail.

## Next steps

The design also places the sidebar and the 3D canvas as floating 24 px panels on the ground, moves the language switch and the update notice into the title bar, and gives Home a project, printer and LAN model drop dashboard. Those are not implemented by this change.
