# Every-element sweep captures (2026-09)

Hand-written inventory of the surfaces the every-element Material Design 3 sweep must show from the
real built artifact, before and after. A row is `pending` until both files exist in this folder and
were taken from the executable named in the header table. A filename in the table that does not
exist on disk is a defect, not a plan.

## Provenance

| Field | Baseline (before) | After |
| --- | --- | --- |
| Source commit | `9e8d005b0` (main, unmodified) | `a55930919` (main, build attempt 30, md3-v106 payload; C++ unchanged since `92cd7bce7`) |
| Executable | `install-dir/bambu-studio.exe`, `BambuStudio.dll` sha256 `e44dd4288e3f46deb4a25eeced2b28f9cc75a5fbb7fe426328841dc3c03cd0c0` (attempt 7 relink of the same source) | `BambuStudio.dll` sha256 `060dc040a5e60739fd20439ca9b7c165db6001abd80cc613ddae57f4e7ef6814` (attempt 28 link, attempt 30 re-installed resources on top; the md3-v106 installer payload) |
| Capture route | hidden Win32 desktop, `PrintWindow` per top-level window, real GPU driver (GL canvases come back blank on this route; canvas surfaces use the Mesa software path) | same, real GPU driver only: the Mesa copy of this payload exited at launch on the capture host when these were taken (root cause and fix recorded in HANDOFF.md §12; fixed after this set), so canvas panes are blank in this set |
| Display scale | 100% (the host's single display; see Limitations) | same |
| Taken | 2026-09-05 | 2026-09-07 04:11 to 04:36 UTC, 132 captures and 24 probe dumps (282 residual findings, all in the frame-minimum class recorded under CJ-005; the attempt-13 set had 300) |

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

## Dialog sweep captures (2026-09-29)

Taken by `scripts/md3/sweep-dialogs.py`, which opens each menu-reachable dialog on a hidden
desktop and writes a layout-probe dump beside each capture. These are the "before" captures of
clipping inventory rows CJ-015, CJ-016, CJ-018 and CJ-019, plus two bilingual captures of text that
stayed English only (column titles and a section header, fixed in `87ec5c44c`).

| Field | Value |
| --- | --- |
| Source commit | `73a952b15` (release `md3-v143`) |
| Executable | the `md3-v143` payload, `bambu-studio.exe` sha256 `c88eea3912b82b9e9aef428bd53a219f4874c010a930ad71e7032b46295ae202`, `BambuStudio.dll` sha256 `ab2f92d88f3778464c327bfce755e10944809e0623a27920b97f8b70af25ef48` |
| Payload note | that release shipped `BambuStudio.dll` without its dependency DLLs and could not start; the OpenCascade, FFmpeg, GMP, MPFR, FreeType and WebView2 loader DLLs of the same dependency build were placed beside it for these captures. The app code in the captures is the release's own. |
| Capture route | hidden Win32 desktop, `PrintWindow` of the dialog, real GPU driver |
| Display scale | 100% |

| File | Dialog | Tuple |
| --- | --- | --- |
| `dialog-smart-home--en-light-comfortable--before.png` | Smart home | `en-light-comfortable` |
| `dialog-keyboard-shortcuts--bilingual_en_yue_HK-light-comfortable--before.png` | Keyboard Shortcuts | `bilingual_en_yue_HK-light-comfortable` |
| `dialog-temperature--bilingual_en_yue_HK-light-comfortable--before.png` | Temperature calibration | `bilingual_en_yue_HK-light-comfortable` |
| `dialog-retraction-test--yue_HK-light-comfortable--before.png` | Retraction test | `yue_HK-light-comfortable` |
| `dialog-check-for-update--bilingual_en_yue_HK-light-comfortable--md3-v143.png` | Newest-version message (before CJ-014) | `bilingual_en_yue_HK-light-comfortable` |
| `dialog-config-profiles-backup--bilingual_en_yue_HK-light-comfortable--md3-v143.png` | Config profiles & backup (before CJ-017) | `bilingual_en_yue_HK-light-comfortable` |
| `dialog-retraction-test--bilingual_en_yue_HK-light-comfortable--before.png` | Retraction test | `bilingual_en_yue_HK-light-comfortable` |
| `dialog-version-history--bilingual_en_yue_HK-light-comfortable--before.png` | Version history | `bilingual_en_yue_HK-light-comfortable` |

## Release md3-v148 captures (2026-09-29)

Taken by `scripts/md3/sweep-dialogs.py` and `scripts/md3/capture-tuple.py` from the unmodified `md3-v148` release package: nothing was
added to its payload, and every import of its executables and DLLs resolves in the package or in
Windows (`scripts/ci/check_payload_imports.py`).

| Field | Value |
| --- | --- |
| Source commit | `3a935ed9f` (release `md3-v148`) |
| Package | `BambuStudioMD3-2.8.4147-full.nupkg`, SHA-1 `ffb45f1e5662f604753c6cbfd1fb148435e53594` as listed in `RELEASES` |
| Executable | `bambu-studio.exe` sha256 `68621a2a1b399327d327dcd63775370aaad61a66319c8d8f625991dc866a3c09`, `BambuStudio.dll` sha256 `91cfa047d572b467f11e4c458f959dab61ecbb11835d3f781201a1df06431d81` |
| Capture route | hidden Win32 desktop, `PrintWindow` of the dialog, real GPU driver |
| Display scale | 100% |

| File | Dialog | Tuple |
| --- | --- | --- |
| `dialog-check-for-update--bilingual_en_yue_HK-light-comfortable--before.png` | Newest-version message (before CJ-020) | `bilingual_en_yue_HK-light-comfortable` |
| `release-md3-v148-prepare--en-light-comfortable.png` | Main window, Prepare page: the release starts with nothing added | `en-light-comfortable` |
| `release-md3-v148-prepare--yue_HK-light-comfortable.png` | Main window, Prepare page | `yue_HK-light-comfortable` |
| `release-md3-v148-prepare--bilingual_en_yue_HK-light-comfortable.png` | Main window, Prepare page | `bilingual_en_yue_HK-light-comfortable` |
| `dialog-smart-home--bilingual_en_yue_HK-light-comfortable--before.png` | Smart home: media buttons "Previous · 上一首" beside "Next · 下一步" (before `1757d880f`) | `bilingual_en_yue_HK-light-comfortable` |

## Release md3-v150 captures (2026-09-29)

Taken by `scripts/md3/sweep-dialogs.py` from the unmodified `md3-v150` release package.

| Field | Value |
| --- | --- |
| Source commit | `74641b8c0` (release `md3-v150`) |
| Package | `BambuStudioMD3-2.8.4149-full.nupkg`, SHA-1 `293c1b652266cd6a190fb8026531121c934676be` as listed in `RELEASES` |
| Executable | `bambu-studio.exe` sha256 `395396bbe14193dee1b9aff04612a2f80af9e52f72662f1d498d6ed0d7b8a9e4`, `BambuStudio.dll` sha256 `3807cf4b1f1268b250db431b01c0f2d6a1f09be720c8c3a15603b99ba3918b35` |
| Capture route | hidden Win32 desktop, `PrintWindow` of the dialog, real GPU driver |
| Display scale | 100% |

| File | Dialog | Tuple |
| --- | --- | --- |
| `dialog-smart-home--en-light-comfortable--md3-v150.png` | Smart home, full search field (after CJ-015) | `en-light-comfortable` |
| `dialog-config-profiles-backup--bilingual_en_yue_HK-light-comfortable--md3-v150.png` | Config profiles & backup (after CJ-017) | `bilingual_en_yue_HK-light-comfortable` |
| `dialog-temperature--bilingual_en_yue_HK-light-comfortable--md3-v150.png` | Temperature calibration (after CJ-018) | `bilingual_en_yue_HK-light-comfortable` |

## Layout-probe dumps

`probe/<tuple>--<before|after>.jsonl`, one per main-frame idle dump plus one per opened dialog,
read with `node ui-md3/tests/layout-probe-report.mjs`. The findings table for each run is recorded
in `docs/features/design-system/cheap-jor-inventory.md`.

## Limitations

- Display scale tuples (125%, 150%, 200%) need a display at that scale. This host has one display
  at 100%, and per-monitor scaling is a user-visible setting that a hidden-desktop run must not
  change. Those tuples are recorded as not run until a display at each scale is available.
- WebView2 panes are captured with headless Edge per HANDOFF §4, not with `PrintWindow`.
