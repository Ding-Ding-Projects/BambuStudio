# Graphite shell design handoff

This folder holds the complete design canvas for the Graphite shell, a redesign of the Bambu Studio MD3 main window. It can be read back without the hosted design project.

## Files

- `canvas.json`: the canvas index (artboard frames and order).
- `Main.dc.html`: Prepare workspace.
- `Home.dc.html`: Home (account and library navigation, recent files, Online Models, Print History, LAN model drop, printers, Home Assistant handover, Melody creator entry).
- `Preview.dc.html`: G-code preview with view types, display options, legend and statistics dock, layer slider and move timeline.
- `Device.dc.html`: camera, print progress and speed levels, temperatures, AMS, control, fans and light, extras.
- `Dialogs.dc.html`: Send print job, Melody creator, command palette with regex search.
- `Tokens.dc.html`: Graphite colour roles for dark and light, type, shape, elevation and motion tokens.
- `TitleBar.dc.html`, `NavRail.dc.html`: shared shell components imported by every screen.
- `FEATURE_PARITY.md`: the feature inventory (217 features) with source location, design location and status.
- `../interface-motion.md`: the motion specification (tokens, interaction map, native mapping, reduced motion).
- `frames/`: renders of every artboard at 1440 px, made with the stand-in runtime described below.
- `REHEARSAL.md`: notes from the Stageshow review and the render check.

The artboards use the Design Component format and load their runtime from `support.js`, which is supplied by the design tool and is not stored here. The markup constructs (`x-dc`, `helmet`, `dc-import`, `sc-for`, `{{ }}` interpolation, the `text/x-dc` script block with `renderVals`) are unchanged from the previous revision, so the design tool can open the files as before.

## What changed in this revision

The owner feedback was that the design did not look modern enough, that some features of the real application looked removed, and that it needed more animation.

1. **Feature parity.** `FEATURE_PARITY.md` inventories every user-facing feature of the rail, title bar, Prepare, Preview, Device, Home and the dialogs from the source: 217 features, of which 88 were already drawn, 118 were missing and are restored, 3 had non-native labels (now native: Multi-device, Calibration, Ink), and 8 are not drawn on purpose with a reason (native menus, conditional printer states). Main restorations: the File, Edit, View, Objects, Calibration and Help menus, the LAN drop, notification and appearance buttons, the Print and Local tools rail destinations, eight missing gizmos (mesh boolean, assembly, text, SVG, brim ears, fuzzy skin, measure, simplify), the object manipulation card, the 3D navigator, split buttons for Slice and Print with their menus, nine Preview display options and five more view types, the legend statistics, the Device camera, safety and speed controls, Z axis, auto homing, extruder load and unload, AMS actions, and the Home navigation, Online Models and Print History.
2. **Modernisation.** Within the existing Graphite tokens (no brand colour changed): a tonal surface ladder with a faint inner edge on cards, 28 px cards and 32 px dialogs, translucent blurred controls floating over the 3D canvas, pill buttons that morph their corners on hover and press, sentence-case 13 px section labels instead of 11 px uppercase, a tonal canvas ground with a lit plate and soft shadows, segmented controls with a sliding thumb, a spring switch, and larger hierarchy contrast in the Home header and the Device readouts.
3. **Motion.** `../interface-motion.md` gains a Graphite shell motion specification, and every artboard carries the matching CSS: rail indicator slide, shared-axis screen entrance, staggered card entrance, card lift and press, state layers, segmented thumb and switch springs, dialog and toast entrances, progress growth with a highlight, live camera pulse, legend and AMS hover. A `prefers-reduced-motion: reduce` block gives the static equivalent.
4. **Frames.** All six frames in `frames/` are re-rendered (see "Rendering" below).

## What is implemented

On `main`: the left navigation rail and the Graphite colour roles from `Tokens.dc.html` in the native kit (`b92da01`), the same roles in every bundled web page, and the Home header and recent-project grid (`029af58`), the floating 3D canvas without the sidebar divider (`292ff65`), the title bar language switch (`d5adb8c`) the title bar update chip, the floating sidebar cards and the Home column with the LAN model drop, printers and melody creator cards, the floating Preview legend, the command palette surface, the title bar search field (it opens the command palette) and the Prepare plate, estimate and Slice/Print controls floating on the window ground; see `docs/features/design-system/graphite-shell.md`.

Still design only, including everything added in this revision: rounded canvas corners (the OpenGL view is a native child window), the Prepare sidebar contents (the Ink list, the Process tiles and the Express print card), the scene toolbar beside the gizmo rail, the Preview control layout (view chips, display options, layer slider and bottom bar), the restyled Device screen, the Send print and Melody creator dialog layouts, the Home navigation column with Online Models and Print History, the title bar menus drawn as text buttons with the new indicator buttons, the extra rail destinations, and all motion in the specification. Native labels keep their existing wording, so the rail reads "Multi-device", "Calibration" and "Ink".

Unresolved wording: the rail label for the monitor page is "Device" in the design and in the command palette, while the tab text in `MainFrame.cpp` is "Monitor". Decide before implementation.

## Design-only items that are not features of the application

- The Workspace rail tile has no destination in the source; it stands for the Project workspace panel.
- The project tab row and its new-tab button are a design addition; the source has a single project chip.
- The Prepare sidebar draws the Ink and Process cards together; the source shows one section at a time through the Prepare section tab strip.
- Copy invite link on the Home LAN card is a design shortcut; the native card offers Show invite and Open settings.

## Rendering

The frames were rendered in Chromium (Playwright) at 1440 px with the fonts the application ships (Roboto, Roboto Mono, Material Symbols Outlined) and with a minimal stand-in for `support.js` kept outside the repository: it expands `dc-import`, `sc-for` and `{{ }}` and runs each component's `renderVals`. To check that the stand-in is faithful, the unmodified previous artboards were rendered through it first and matched the committed frames. The stand-in is not the design tool runtime, so these frames are renders of the design artboards, not captures of the running application, and open items below mention what the design tool itself has not yet confirmed. Frames show the end of the entrance sequence (the renders wait 2.5 s); the reduced-motion render of Preview was inspected and matches its end state (Main was rendered in that mode without errors but not inspected).

## Design review

Reviewed against the Stageshow skill (benjitaylor/skills, folder `stageshow`, installed fresh for this pass with `npx skills add`; lock hash `730b32b96c3d290d37e0a7d2a62193e33a9314328313d6968740b6a817c8af61`). References read: `SKILL.md`, `references/review.md`. See `REHEARSAL.md` for what was applied and what was not.

Fixed in the review rounds of this pass:

1. Title bar: with the six menus and the three new indicator buttons the bar overflowed at 1440 px and squeezed the search field. The wordmark moved into the brand tile tooltip and the extra project tab was removed, so the search field reads in full.
2. Preview: the display options row ran under the legend dock, so it now wraps; the layer value bubble no longer wraps; the model no longer sits under the bottom bar.
3. Dialogs: the three dialogs overflowed the 1440 px canvas by 48 px, caused by padding on a content-box root; fixed.
4. Home: the Maker's Supply entry wrapped; the printers card now fills the column instead of leaving a gap above the Melody creator card.

Still open:

- Narrow widths: the shell is a desktop window with a minimum width near 1000 px; the title bar at 1440 px is already full and below about 1300 px the search field, then the history chip, would have to hide as `BBLTopbar.cpp` already does.
- Not checked: Cantonese and bilingual text fitting (several controls, such as the Print speed levels and the Preview option chips, are wider in Cantonese), light theme artboards, keyboard order, scales other than 100 %.
- The Home navigation column ends well above its bottom when there are few entries.
- The motion is specified and drawn in the artboards only. No native animation was implemented and no running application was captured.
- Several source strings were read at file level only: the Prepare sidebar process groups, the Device print options and the Print workflow panel contents were not opened in detail.
