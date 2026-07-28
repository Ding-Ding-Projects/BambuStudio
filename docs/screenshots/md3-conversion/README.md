# MD3 conversion captures

Native captures from the real built binary through `.claude/skills/run-bambustudio/`, taken on
2026-07-28 after the stock-UI conversion waves.

| File | Surface | Build |
| --- | --- | --- |
| `prepare-workspace-dark-D06044.png` | Prepare workspace, dark mode, 846x1279 | DLL `D0604414AAFD14BB65B9CA4F996C9F6900A38CEE0B4550D17731A1C6FBBD6D52`, 151,318,016 bytes, 2026-07-28 16:52:43 |

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

## Still owed

Captures of the surfaces that need a device connection or a loaded model, none of which were
reachable in this pass:

- the **fan control popup** — the largest single reskin, and the one whose `wxStaticBitmap` PNG
  pseudo-switches became real `SwitchButton` controls, so it also wants a keyboard-focus check;
- the **Slice / Print dropdowns** (`SideButton` defaults + the new `SideMenuPopup` surface);
- the **measurement gizmo chips in dark mode**;
- the **2D bed preview in dark mode**, which carries an explicit open question recorded at
  `src/slic3r/GUI/2DBed.cpp:88-100`: the slab sits at 1.05:1 against its backdrop by arithmetic,
  and every role pairing that raises it costs grid contrast. Only a capture settles that trade.

`driver.py open --model` spawns a **second** instance that times out at GL init under llvmpipe;
load the model into the first instance instead.
