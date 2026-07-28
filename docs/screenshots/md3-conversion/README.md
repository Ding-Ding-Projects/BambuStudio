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

> [!IMPORTANT]
> The bottom action bar clips — the slice button reads **"Slice pl…"**. **That is an artefact of
> this host, not a defect.** `GUI_App::get_min_size()` declares a 1000x600 minimum, this machine's
> primary display is **832 x 1573**, and `create_headless_desktop` inherits that resolution with no
> override. The frame is therefore forced to ~846 wide — below the app's own supported minimum —
> and any layout measured there is measuring an unsupported size.
>
> **Main-frame clipping cannot be verified at a supported width on this machine.** Dialog-level
> review still works, because dialogs are smaller than the frame; that is how the Smart Home
> 720x760 and 520x480 reviews were done.

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

## The main-frame clip is a real defect, root-caused — just not fixable blind

The `"Slice pl…"` clip in `prepare-workspace-dark-D06044.png` was chased to its cause rather than
dismissed as a host artefact. It is **both**: this host triggers it, and the mechanism is in the app.

`GUI_App::get_min_size()` (GUI_App.cpp:4258) declares a **1000 x 600** minimum. But
`GUI_App::window_pos_sanitize()` (GUI_App.cpp:8241) runs
`WindowMetrics::sanitize_for_display()`, whose first act is
`rect = rect.Intersect(screen_rect)` (GUI_Utils.cpp:373). The intersect wins: on a display narrower
than the declared minimum, **the app clamps itself below its own minimum** and the bottom action bar
overflows, with nothing on screen indicating it.

Measured on this host via `WM_GETMINMAXINFO` on the live frame:

```
minTrackSize  832 x 1279      <- the app's own reported minimum, i.e. the screen width
maxTrackSize  846 x 1539
```

So the frame is pinned between 832 and 846 px wide **by the app**, not by the window manager.
`SetWindowPos` and `MoveWindow` to 1200 are both refused, restoring from maximized first changes
nothing, and the app never was maximized.

> [!WARNING]
> **This was deliberately NOT blind-fixed.** The action bar is a high-traffic surface, and this host
> can only render it at 832-846 px — an atypical width. Changing a prominent layout when the only
> available verification is one unusual width is how a rare bug gets traded for a common one, which
> already happened twice in this work (a contrast fix that painted a magenta hover underline, and an
> MD3 conversion that introduced `overflow: hidden` clipping). The fix needs a >= 1000 px display to
> validate the normal case.
>
> Two escapes from the display limit were tried and both are closed here: `create_virtual_display`
> requires **Xvfb** and is Linux-only, and `create_headless_desktop` takes no resolution argument —
> it inherits the session's. Changing the host's display mode would disturb the user's own session,
> so it was not done. (`Win32_VideoController` reports the adapter at 1280x800, but the session and
> every headless desktop created from it report 832x1573.)

## One blocker that turned out not to be real

`driver.py open --model` reports `no studio log contained 'finished init opengl' within 240s` and
looks like a hard failure. **It is not** — that is the driver's log wait, not the app. Verified
2026-07-28: the same launch produced a usable `* Untitled - BambuStudio` frame **20 seconds** after
the command reported failure. Poll `driver.list_windows()` for a new frame instead of trusting the
error. Recorded in `.claude/skills/run-bambustudio/SKILL.md`; `prepare-model-loaded-dark-D06044.png`
is the result.
