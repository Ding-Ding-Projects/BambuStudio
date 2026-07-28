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

## Still owed, and exactly what blocks each

These were attempted in this pass and are **blocked by this host or this configuration**, not by
effort. Each needs the stated unblock before it can be captured honestly.

| Surface | Why it matters | Blocked by |
| --- | --- | --- |
| **Fan control popup** | The largest single reskin. Its `wxStaticBitmap` PNG pseudo-switches became real `SwitchButton` controls, so it also needs a **keyboard-focus check** — previously that popup was mouse-only | Needs a **connected printer**; the Device workspace has no reachable path without one |
| **Slice / Print dropdowns** | `SideButton`'s constructor defaults were the legacy palette, which is why these dealt out solid brand-green bars; `SideMenuPopup` had no surface at all | **Model loading is solved** (see below) — what remains is that `SidePopup` is a `wxPopupTransientWindow`: any process spawned on the desktop while it is open focus-kills it, so it needs `popovercap.py` pointed at the caret's **own** child hwnd, not the frame |
| **Measurement gizmo chips, dark** | The chips were 50%-alpha white with `OnSurface` text — near-white on near-white | Model loads fine now; the gizmo needs the object actually selected first, and the cube is only a few pixels at the default camera. Zoom in or place a larger model before activating Measure |
| **2D bed preview, dark** | Carries an explicit open question at `src/slic3r/GUI/2DBed.cpp:88-100`: the slab sits at **1.05:1** against its backdrop by arithmetic, and every role pairing that raises it costs grid contrast. Only a capture settles the trade | `BedShapeDialog` is the only `Bed_2D` call site, and **bed shape is not exposed for Bambu printer profiles** — the profile fixes it. Needs a custom/third-party printer profile |
| **Settings search popover** | A required surface (every settings page must route search through the shared regex builder) | It is a **transient popover**: any process spawned on the headless desktop while it is open focus-kills it. Must be driven with `popovercap.py` in a single on-desktop process |

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
