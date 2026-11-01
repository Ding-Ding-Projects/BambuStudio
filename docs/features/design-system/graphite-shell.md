# Graphite shell

The Graphite shell is a redesign of the main window chrome. It changes where the workspaces live and the neutral palette every Material Design 3 surface paints from. Slicing, printing, presets, projects and every feature of this fork are unchanged.

## What changed

- **Navigation rail.** The main workspace book (`MainFrame::m_tabpanel`) docks left with `wxNB_LEFT`. `ButtonsListCtrl` lays its destinations out as an 88 px rail: each destination is a 72 x 60 px tile with the Material Symbols glyph above the label, the selected tile takes the secondary container, and a short primary bar on the leading edge is the non-colour cue. Navigation actions (Settings) are pinned to the bottom. When the window is too short, the overflow menu takes the tail exactly as the strip did, measured along the height with the same `NotebookNavigation::fit()`.
- **Every other notebook keeps the top strip.** A notebook created without a dock style still defaults to `wxBK_TOP`, so the settings dialog and its sub-tabs are untouched.
- **Graphite palette.** The neutral ramp in `MD3Tokens.hpp` becomes a green-tinted graphite. Dark: ground `#0e1311`, containers `#0a0e0c` to `#28322c`, text `#e3ebe5` and `#a9b6ad`, primary `#6fdc98`. Light: ground `#f5f8f5`, containers `#ffffff` to `#d6e1da`, text `#131c17` and `#3f4c44`, primary `#146c2e`. Text keeps at least 4.5:1 and outlines at least 3:1 against every surface in both themes, and the dark compatibility map stays idempotent (`tests/native_navigation/neutral_palette.test.mjs`).
- **Floating canvas.** The 3D canvas (Prepare, Preview and Assemble) sits on the window ground with a gutter of one density gap on every side, and the hairline between the sidebar and the canvas is gone. The canvas corners stay square because the OpenGL view is a native child window.
- **Language switch in the title bar.** A three-segment switch (EN, 粵, 雙) sits before the window controls and shows the current language mode. Choosing another segment runs the same steps as Preferences: an offer to save an unsaved project, the restart confirmation, the modified-preset check, then the interface is rebuilt in the new mode. Any other installed language leaves no segment highlighted.
- **Web pages follow the palette.** Every bundled web page that carries its own copy of the colour roles (Home, the device page, the filament manager and creator, the guide, login and the model pages) now uses the Graphite values, so web surfaces match the native ones.
- **Home.** The Home page drops the gradient hero band for a plain display header with 16 px action buttons, and shows up to six recent projects as a three-column grid inside one 24 px panel. These renders come from the shipped page opened in Chromium with sample recent files, not from the running application:

  ![Home, dark](../../screenshots/graphite-shell/home--en-dark--browser-render.png)
  ![Home, light](../../screenshots/graphite-shell/home--en-light--browser-render.png)
- **New tokens.** `navigation_rail_width`, `navigation_rail_item_width`, `navigation_rail_item_height` and `radius_panel` (24 px, for floating panels and the canvas frame).

## Verification

- `g++ -fsyntax-only` against wxWidgets 3.2, Boost and OpenCASCADE headers on Linux passes for `Notebook.cpp` and `MainFrame.cpp`.
- `tests/native_navigation/graphite_rail.test.mjs` pins the left dock, the top-strip default for other notebooks, the rail geometry and the Graphite grounds.
- The node contract suite reports the same failures as `main` and no new ones. The fingerprint of `MainFrame::init_tabpanel` in `tests/native_shared_controls/atlas_shell_preserved_functions.json` was refreshed because this change deliberately edits that function.

Not verified yet: the hosted Windows build, the running application, captures in English, Cantonese and bilingual modes at each scale, and the keyboard order through the rail.

## Next steps

The design also turns the sidebar sections into floating 24 px panels and rounds the canvas frame, moves the update notice into the title bar, adds the printer and LAN model drop column to Home, and restyles Preview, Device and the dialogs. Those are not implemented yet. The full design is in `design/graphite-shell/`.
