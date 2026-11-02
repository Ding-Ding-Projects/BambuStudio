# Graphite shell design handoff

This folder holds the complete design canvas for the Graphite shell, a redesign of the Bambu Studio MD3 main window. It can be read back without the hosted design project.

## Files

- `canvas.json`: the canvas index (artboard frames and order).
- `Main.dc.html`: Prepare workspace.
- `Home.dc.html`: Home (recent projects with revisions, LAN model drop invite, printers, Home Assistant handover, Melody creator entry).
- `Preview.dc.html`: G-code preview with legend dock, layer slider and move timeline.
- `Device.dc.html`: camera, temperatures and fans, AMS, motion, melody and Home Assistant extras.
- `Dialogs.dc.html`: Send print job with Express print mode, Melody creator, command palette with regex search.
- `Tokens.dc.html`: Graphite colour roles for dark and light, type, shape and accents.
- `TitleBar.dc.html`, `NavRail.dc.html`: shared shell components imported by every screen.

The artboards use the Design Component format and load their runtime from `support.js`, which is supplied by the design tool and is not stored here.

## What is implemented

On `main`: the left navigation rail and the Graphite colour roles from `Tokens.dc.html` in the native kit (`b92da01`), the same roles in every bundled web page, and the Home header and recent-project grid (`029af58`), the floating 3D canvas without the sidebar divider (`292ff65`), the title bar language switch (`d5adb8c`) the title bar update chip, the floating sidebar cards and the Home column with the LAN model drop, printers and melody creator cards, and the floating Preview legend; see `docs/features/design-system/graphite-shell.md`. Still design only: rounded canvas corners (the OpenGL view is a native child window), the title bar search field, the Preview control layout (view chips, layer slider and bottom bar), the restyled Device screen, and the dialogs. Native labels keep their existing wording, so the rail reads "Multi-device", "Calibration" and "Ink" where the design says "Fleet", "Calibrate" and "Inks".

## Design review

Reviewed against the Stageshow skill (benjitaylor/skills, commit `8020574`, folder `stageshow`). References read: `SKILL.md`, `references/review.md`, `references/craft.md` (Restraint, Spacing and alignment, Icons and logos). Every artboard was rendered at 1440 px with the fonts the application ships (Roboto, Roboto Mono, Material Symbols Outlined) and reviewed from the frames in `frames/`. The rehearsal notes are in `REHEARSAL.md`.

Fixed in this pass:

1. Icons. Every hand-drawn icon and every Unicode symbol used as an icon is replaced by Material Symbols, the set the application ships. The title bar mark is the application's own brand tile (the `deployed_code` glyph on the primary tile, as `BBLTopbar.cpp` draws it).
2. Prepare. The scene toolbar overlapped the object list; it now starts beside the gizmo rail. The Process card no longer stretches over an empty band.
3. Home. Project thumbnails fill the grid instead of leaving the lower half of the card empty. Printers are tiles with a progress bar. The greeting subtitle and part of the LAN copy were cut.
4. Device. The camera hero takes the free height; the print sits above the status card. Temperatures are three dashboard tiles with the reading as the largest text, and the fans moved to Extras. The chamber light row was cut.
5. Preview. The layer stack sits on the plate.
6. Dialogs. Checked options render as checked, the express line is one sentence, and the command palette sits under the Melody creator.

Still open:

- The AMS and Motion cards on Device end about 90 px above their bottoms at 1440 x 900.
- Narrow widths: the shell is a desktop window with a minimum width near 1000 px, so the 390 px frames are not a target; the title bar collides there.
- Not checked: Cantonese and bilingual text fitting, light theme artboards, keyboard order.
