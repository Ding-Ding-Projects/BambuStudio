# MD3 conversion captures

Native captures from the real built binary through `.claude/skills/run-bambustudio/`, taken on
2026-07-28 after the stock-UI conversion waves.

All captures below come from DLL
`D0604414AAFD14BB65B9CA4F996C9F6900A38CEE0B4550D17731A1C6FBBD6D52`,
151,318,016 bytes, built 2026-07-28 16:52:43 with 0 errors.

| File | Surface |
| --- | --- |
| `prepare-workspace-dark-D06044.png` | Prepare workspace, dark mode, 846x1279 |
| `printer-settings-dark-D06044.png` | Printer settings dialog, dark mode, 750x600 |

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
| **Slice / Print dropdowns** | `SideButton`'s constructor defaults were the legacy palette, which is why these dealt out solid brand-green bars; `SideMenuPopup` had no surface at all | Needs a **loaded model**. `driver.py open --model` spawns a *second* instance that times out at GL init under llvmpipe, and `File > Import` opens a native file dialog that blocks on the headless desktop |
| **Measurement gizmo chips, dark** | The chips were 50%-alpha white with `OnSurface` text — near-white on near-white | Same model-load blocker |
| **2D bed preview, dark** | Carries an explicit open question at `src/slic3r/GUI/2DBed.cpp:88-100`: the slab sits at **1.05:1** against its backdrop by arithmetic, and every role pairing that raises it costs grid contrast. Only a capture settles the trade | `BedShapeDialog` is the only `Bed_2D` call site, and **bed shape is not exposed for Bambu printer profiles** — the profile fixes it. Needs a custom/third-party printer profile |
| **Settings search popover** | A required surface (every settings page must route search through the shared regex builder) | It is a **transient popover**: any process spawned on the headless desktop while it is open focus-kills it. Must be driven with `popovercap.py` in a single on-desktop process |

> [!IMPORTANT]
> **Main-frame clipping cannot be verified on this host at all** — see the note above. That is a
> hardware limit (832 px display vs a 1000 px declared minimum), not an outstanding task. Anyone
> continuing this work needs a machine with a display at least 1000 px wide before main-frame
> clipping claims mean anything.
