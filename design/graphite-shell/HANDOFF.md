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

On `main` at `b92da01`: the left navigation rail and the Graphite colour roles from `Tokens.dc.html` (see `docs/features/design-system/graphite-shell.md`). Everything else on these artboards is design only: floating 24 px panels for the sidebar and canvas, the title bar layout (language switch and update notice), the Home dashboard, the restyled Preview and Device screens, and the dialogs. Native labels keep their existing wording, so the rail reads "Multi-device", "Calibration" and "Ink" where the design says "Fleet", "Calibrate" and "Inks".

## Design review

Reviewed against the Stageshow skill (benjitaylor/skills, commit `8020574`, folder `stageshow`). References read: `SKILL.md`, `references/review.md`, `references/craft.md` (Restraint, Spacing and alignment, Icons and logos). Stageshow targets marketing illustrations rather than application screens, so these rules did not apply: the gold-example copy step, the cursor choreography, filming with `scripts/film.mjs`, the 358 px compact redesign and `REHEARSAL.md`. No frames were filmed; the review below is from the source.

Findings, most important first:

1. Hand-drawn icons. Fixed on the navigation rail and the Prepare gizmo rail, which now use Material Symbols, the set the application ships. Still open: hand-drawn icons in the title bar, Home, Preview, Device and dialogs, and Unicode arrows and symbols used as icons on the Device motion pad, the window controls and the Preview play button.
2. Logo. The title bar shows a cube in a mint square, not the application's own logo. Use the shipped logo asset.
3. Density. Home and Device carry many real strings at once; reduce secondary text where the screen is not about it.
4. Not checked: rendering at narrow widths, text fitting in Cantonese and bilingual modes, and contrast of the accent chips beyond the token sheet.
