# Every-element sweep captures (2026-09)

Hand-written inventory of the surfaces the every-element Material Design 3 sweep must show from the
real built artifact, before and after. A row is `pending` until both files exist in this folder and
were taken from the executable named in the header table. A filename in the table that does not
exist on disk is a defect, not a plan.

## Provenance

| Field | Baseline (before) | After |
| --- | --- | --- |
| Source commit | `9e8d005b0` (main, unmodified) | pending |
| Executable | `install-dir/bambu-studio.exe`, `BambuStudio.dll` sha256 `e44dd4288e3f46deb4a25eeced2b28f9cc75a5fbb7fe426328841dc3c03cd0c0` (attempt 7 relink of the same source) | pending |
| Capture route | hidden Win32 desktop, `PrintWindow` per top-level window, real GPU driver (GL canvases come back blank on this route; canvas surfaces use the Mesa software path) | same |
| Display scale | 100% (the host's single display; see Limitations) | same |

## Tuples

Language: `en`, `yue_HK`, `bilingual_en_yue_HK`. Theme: light, dark. Density: comfortable, compact.
Each surface below is captured at every tuple; the file name is
`<surface>--<language>-<theme>-<density>--<before|after>.png`.

## Surfaces

| Surface | How it is reached | Status |
| --- | --- | --- |
| home | main frame on the Home tab after first idle | pending |
| prepare | Prepare tab, empty plate, simple sidebar | pending |
| prepare-advanced | Prepare tab, advanced sidebar | pending |
| preview | Preview tab after slicing the sample cube | pending |
| device | Device tab, no printer connected | pending |
| calibration | Calibration tab | pending |
| multi-device | Multi-device tab | pending |
| project | Project tab | pending |
| preferences-general | Preferences dialog, first tab | pending |
| preferences-appearance | Preferences dialog, appearance tab | pending |
| create-presets | Create printer preset dialog, page 1 | pending |
| save-preset | Save preset dialog | pending |
| feed-direction | AMS feed direction dialog | pending |
| calibration-preset-page | Calibration wizard preset page | pending |
| unsaved-changes | Unsaved changes dialog | pending |
| update-dialog | Update available dialog | pending |
| msg-dialog | Message dialog with a script body | pending |
| send-system-info | Send system info dialog | pending |
| network-test | Network test dialog | pending |
| smart-home | Smart home dialog | pending |
| mixed-filament | Mixed filament dialog | pending |
| pdf-export | Assembly PDF export dialog | pending |
| fan-control | Fan control popup | pending |
| slice-dropdown | Slice / Print dropdown | pending |
| regex-builder | Regex builder popover from the sidebar search | pending |
| command-palette | Command palette | pending |

## Layout-probe dumps

`probe/<tuple>--<before|after>.jsonl`, one per main-frame idle dump plus one per opened dialog,
read with `node ui-md3/tests/layout-probe-report.mjs`. The findings table for each run is recorded
in `docs/features/design-system/cheap-jor-inventory.md`.

## Limitations

- Display scale tuples (125%, 150%, 200%) need a display at that scale. This host has one display
  at 100%, and per-monitor scaling is a user-visible setting that a hidden-desktop run must not
  change. Those tuples are recorded as not run until a display at each scale is available.
- WebView2 panes are captured with headless Edge per HANDOFF §4, not with `PrintWindow`.
