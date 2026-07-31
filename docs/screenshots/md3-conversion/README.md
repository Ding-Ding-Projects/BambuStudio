# MD3 conversion captures

Native captures from the real built binary through `.claude/skills/run-bambustudio/`, taken on
2026-07-28 after the stock-UI conversion waves.

All captures below come from DLL
`D0604414AAFD14BB65B9CA4F996C9F6900A38CEE0B4550D17731A1C6FBBD6D52`,
151,318,016 bytes, built 2026-07-28 16:52:43 with 0 errors.

| File | Surface |
| --- | --- |
| `prepare-workspace-dark-D06044.png` | Prepare workspace, dark mode, 846x1279 |
| `prepare-model-loaded-dark-D06044.png` | Same workspace with `cube.stl` loaded — the info toast confirms 20x20x20 mm / 12 triangles, and the slice action is live |
| `printer-settings-dark-D06044.png` | Printer settings dialog, dark mode, 750x600 |
| `gizmo-rail-glyphs-dark-D06044.png` | The gizmo rail, 2x. Every entry is a clean monochrome Material Symbols glyph — this is `gizmo-rail-svg-icons`' GL-atlas half visible, with no legacy raster and no hand-authored `_dark` twin |

The printer-settings capture shows the dialog's MD3 chrome end to end: the tokenised
surface, the tab row (`Basic information` / `Motion ability` / `Extruder`) with Material
Symbols, the preset combo, the save and search affordances, and the `Advanced` switch.

## What the Prepare capture shows

- MD3 dark surfaces, Material Symbols glyphs and the tab strip throughout.
- Both sidebar search bars (**Search inks**, **Search settings**) carrying the `.*` regex toggle and
  the tune affordance that opens the shared regex builder.
- The gizmo rail rendering through the glyph bridge.
- **`OBJECT MANIPULATION`'s X / Y / Z labels still red / green / blue.** This is the visible proof
  that the axis-colour revert holds: axis colours are exempt data, and the 3D gizmo draws pure RGB,
  so tokenising the 2D side would have made the panel disagree with the scene.

## What it does NOT show, and why

> [!NOTE]
> These two Prepare captures predate the action-bar fix, so their bottom bar still reads
> **"Slice pl"** with no Print button. That was **not** a host artefact, though it was twice
> recorded here as one — see "The action-bar clip" below for the real cause and the after shot.
>
> The host limit that *is* real: `GUI_App::get_min_size()` declares a 1000x600 minimum, this
> machine's primary display is **832 x 1573**, and `create_headless_desktop` inherits that
> resolution with no override, so the frame is pinned at ~846 wide. **Main-frame layout at a
> supported width cannot be verified here.** Dialog-level review still works, because dialogs are
> smaller than the frame; that is how the Smart Home 720x760 and 520x480 reviews were done.

## Photographs still owed — these surfaces are FIXED, not outstanding

> [!IMPORTANT]
> **Every surface below is already fixed, compiled and shipped.** This table tracks missing
> *photographs*, not missing work. An earlier revision of this section was worded as a to-do list
> and was reasonably misread as "these defects are unresolved" — they are not.

| Surface | Fix, and where it landed | Why no photo yet |
| --- | --- | --- |
| **Fan control popup** | `Widgets/FanControl.cpp` — 1161 lines that had **zero** `MD3::Role` references, now tokenised; its `wxStaticBitmap` PNG pseudo-switches are real `SwitchButton` controls, so the popup gained keyboard operation and screen-reader state. Commit `b13cf772e` | Needs a **connected printer**; the Device workspace has no reachable path without one |
| **Slice / Print dropdowns** | `Widgets/SideButton.cpp` (legacy palette was the *constructor default*, which is why the rows were solid brand-green bars) and `Widgets/SideMenuPopup.cpp` (the menu drew no surface, border or radius at all). Commit `b13cf772e` | `SidePopup` is a `wxPopupTransientWindow`: any process spawned on the headless desktop while it is open focus-kills it. Needs `popovercap.py` aimed at the caret's **own child hwnd** — aiming it at the frame times out |
| **Measurement gizmo chips, dark** | `Gizmos/GLGizmoMeasure.cpp` — both value chips were 50%-alpha pure white with `OnSurface` text (near-white on near-white in dark mode); now `SurfaceContainer` with kit radius and an `OutlineVariant` hairline. Commit `c03e716a8` | Selection works (`press.py press "Select all"`, and the Move/Rotate/Scale gizmos light up) but the Measure gizmo does not open from a synthetic click on the rail. Needs a working activation path — try its keyboard shortcut rather than the rail glyph |
| **2D bed preview, dark** | `2DBed.cpp` — chrome tokenised, and the X/Y axis arrows **reverted to pure red/green** because axis colours are exempt data and the 3D gizmo still draws pure RGB. Commit `bbdcd140d` | `BedShapeDialog` is the only `Bed_2D` call site and **bed shape is not exposed for Bambu printer profiles** — the profile fixes it. Needs a custom/third-party printer profile |
| **Settings search popover** | Pre-existing surface; captured here only as evidence that every settings page routes search through the shared regex builder | Transient popover, same focus-kill problem as the Slice dropdown |

The 2D bed contrast question is **settled, and it passes** — numerically, which is a better answer
than a screenshot because WCAG is a numeric standard an eye cannot adjudicate.

| Pair | Light | Dark | Applicable rule | |
| --- | --- | --- | --- | --- |
| Contour ring vs bed fill — **the boundary that says where the printable area is** | 4.47:1 | 6.23:1 | 1.4.11 non-text, 3:1 | PASS |
| Dimension annotation vs backdrop | 8.92:1 | 10.87:1 | 1.4.3 text, 4.5:1 | PASS |
| Bed fill vs backdrop | 1.05:1 | 1.09:1 | none — see below | n/a |

The 1.05:1 figure was read more than once as an AA failure. It is not one: WCAG 1.4.11 governs
user-interface components and graphical objects required to understand content, and two adjacent
decorative surface tones are neither. Raising it would cost grid separation, which is why the slab
deliberately keeps the lowest container.

Pinned in `ui-md3/tests/md3-conversion-contracts.test.mjs`, which reads the roles `2DBed.cpp`
actually assigns rather than the palette — an earlier version asserted against the tokens alone and
a mutation that re-pointed the contour role slipped straight through it.

## The action-bar clip: found, root-caused, fixed, proven

| | |
| --- | --- |
| `action-bar-before-starved-row.png` | **"Slice pl"** clipped, and **no Print button at all** |
| `action-bar-after-starved-row.png` | **"Slice plate"** and **"Print plate"** both whole, at the same 846 px |

This was twice dismissed as an artefact of the 832 px display. It was a real defect.

`update_prepare_action_bar_content()` sized the canvas-alignment spacers to the **full** sidebar
width (344 px). Those spacers are **proportion-0** sizer items and the tool row is **proportion-1**.
When a row cannot fit every minimum, `wxBoxSizer` takes its degenerate branch (`sizer.cpp:2253`):
it pays the **fixed** items first (`:2257-2269`) and gives the proportional ones only the remainder
(`:2274-2286`). The spacer took its 344 px; the tool row was left 214 px short, and that cascaded
through three nested sizers into a **92 px** Slice pill and a **0 px** Print pill.

> [!IMPORTANT]
> **It never looks like overflow, and that is what made it hard.** wx truncates the straddling item
> and allocates **zero** to everything after it (`GetMinOrRemainingSize`, `sizer.cpp:2162-2190`), so
> every child still reports a rect comfortably *inside* the frame. Measuring the children and
> concluding "nothing overhangs, so nothing is clipped" is precisely the wrong inference — the
> starved control has not spilled, it has been erased. A whole primary action was missing from every
> capture for hours and read as "a slightly narrow button".

The fix lets the spacers claim only what the row does not need — cosmetic alignment with the 3D
canvas never outranks a primary action — and logs a warning when the row is still over-subscribed,
so the next starved control announces itself instead of disappearing.

Two earlier attempts were **inert and were reverted**: forcing the frame past its minimum (Windows
caps a top-level window at the work area, `maxTrackSize` 846), and zeroing the estimate column
minimum (the bar was not overflowing in the way that would have helped).

> Escapes from the display limit, both closed here: `create_virtual_display` requires **Xvfb** and is
> Linux-only, and `create_headless_desktop` inherits the session resolution. Changing the host
> display mode would disturb the user's own session, so it was not done. Main-frame layout at a
> *supported* width still cannot be verified on this machine.

## One blocker that turned out not to be real

`driver.py open --model` reports `no studio log contained 'finished init opengl' within 240s` and
looks like a hard failure. **It is not** — that is the driver's log wait, not the app. Verified
2026-07-28: the same launch produced a usable `* Untitled - BambuStudio` frame **20 seconds** after
the command reported failure. Poll `driver.list_windows()` for a new frame instead of trusting the
error. Recorded in `.claude/skills/run-bambustudio/SKILL.md`; `prepare-model-loaded-dark-D06044.png`
is the result.
