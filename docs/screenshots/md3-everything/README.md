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

## Privacy

Everything in this folder is public. The app shows folders on screen (Config profiles & backup lists
the data folder, Preferences the download folder), and a layout dump records the text of every field,
so a capture profile inside a Windows user profile puts the account name into the evidence. Capture
profiles therefore live outside every user profile, under `C:\Users\Public\bbsdd`
(`scripts/md3/prepare-capture-datadirs.py` refuses a root inside the profile and keeps each profile's
downloads in its own folder), and `ui-md3/tests/evidence-privacy.test.mjs` fails on any text evidence
here that names a folder under a user profile. On 2026-09-29 the two Config profiles & backup
captures (`md3-v143`, `md3-v150`) were taken again from the same payloads with such a profile,
showing the same layout, and the download folder recorded in the twelve 2026-09-06/07
`*--after-preferences.jsonl` dumps was replaced with `<redacted: local user folder>`.

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

## Release md3-v151 captures (2026-09-29)

Taken by `scripts/md3/capture-tuple.py` from the unmodified `md3-v151` release package, the first
release published as latest by the release job's newer-than-latest rule. Every import of its
executables and DLLs resolves in the package or in Windows (`scripts/ci/check_payload_imports.py`).

| Field | Value |
| --- | --- |
| Source commit | `ca2b6e101` (release `md3-v151`) |
| Package | `BambuStudioMD3-2.8.4150-full.nupkg`, 732,138,501 bytes, SHA-1 `6c6031131b79e06836825c9d4442f18fda5d7239` as listed in `RELEASES` |
| Executable | `bambu-studio.exe` sha256 `b1c47e411720bf36aa5b72e3939069456b656a6bcd1366abbd07d4f248be8e05`, `BambuStudio.dll` sha256 `f22c4e9a34e6322d990ece8f91a55ca8c70caa8f168777bbbf7e268a1c3e378f` |
| Capture route | hidden Win32 desktop, `PrintWindow` of the window, real GPU driver |
| Display scale | 100% |

| File | Surface | Tuple |
| --- | --- | --- |
| `preferences-general--en-light-comfortable--md3-v151.png` | Preferences, General page: every row's control in view | `en-light-comfortable` |
| `preferences-general--bilingual_en_yue_HK-light-comfortable--md3-v151.png` | Preferences, General page (before CJ-021): a sideways scrollbar, and the Language list, the Funny level sliders, the switch and the Login Region list pushed past the right edge | `bilingual_en_yue_HK-light-comfortable` |
| `preferences-search-update--en-light-comfortable--md3-v151.png` | Preferences searched for "Update automatically": the automatic-update row, switched on | `en-light-comfortable` |
| `preferences-search-update--yue_HK-light-comfortable--md3-v151.png` | Preferences searched for 自動更新: the same row in Cantonese; its wrapped description starts the second line with "。" (fixed in `c7309b889`) | `yue_HK-light-comfortable` |
| `preferences-search-update--bilingual_en_yue_HK-light-comfortable--md3-v151.png` | The same row in bilingual mode: the title reads "Update automatically · 自動更新", the description shows English only (`2e80091ef` puts its Cantonese below it; not yet captured from a release) | `bilingual_en_yue_HK-light-comfortable` |

| `prepare--en-light-comfortable--md3-v151.png` | Main window, Prepare, full process-settings tree (before CJ-023): every value field runs past the sidebar edge | `en-light-comfortable` |
| `prepare--yue_HK-light-comfortable--md3-v151.png` | The same in Cantonese (before CJ-023 and CJ-024): the title reads "打印設…" and the fourth category pill sits half outside | `yue_HK-light-comfortable` |
| `preferences-search-autofill--en-light-comfortable--md3-v151.png` | Preferences searched for "Auto-fill": the row has no description | `en-light-comfortable` |
| `preferences-search-autofill--yue_HK-light-comfortable--md3-v151.png` | The same row in Cantonese (before CJ-022): its description is the whole English catalog header, "Project-Id-Version: Bambu Studio" to "Plural-Forms" | `yue_HK-light-comfortable` |

The two `preferences-search-autofill` files were taken by `scripts/md3/capture-preferences-search.py`.
The three `preferences-search-update` files were taken with the steps of
`scripts/md3/capture-preferences-search.py`, which types the query into the Preferences search field
instead of scrolling: the row sits below the fold of the General page. The committed script reproduces
the bilingual file byte for byte (SHA-256
`f9d15a3ab87c828e26ee6e939b2a2292f810509e77c41cf6ae23b784ace48b42`).

## Release md3-v153 captures (2026-09-29)

Taken from the unmodified `md3-v153` release package: the splash by `scripts/md3/capture-splash.py`
(the release is the first with the splash's capture hook, so each file is the exact bitmap the splash
showed), the dialogs by `scripts/md3/sweep-dialogs.py`. Every import of its executables and DLLs
resolves in the package or in Windows (`scripts/ci/check_payload_imports.py`).

| Field | Value |
| --- | --- |
| Source commit | `1757d880f` (release `md3-v153`) |
| Package | `BambuStudioMD3-2.8.4152-full.nupkg`, 732,142,810 bytes, SHA-1 `3a97afd5d4e083e92a40c72d2c62310e66dcd8c2` as listed in `RELEASES` |
| Executable | `bambu-studio.exe` sha256 `30c71437527d54aaef42c59248245d1cc93a6b96f71047ae676f711a55c05b2d`, `BambuStudio.dll` sha256 `59336cca697a63a936258641b26fe4adceaf304c409ae96fe665f7020b965feb` |
| Capture route | hidden Win32 desktop; splash from the probe's `splash.png`, dialogs by `PrintWindow` |
| Display scale | 100% |

| File | Surface | Tuple |
| --- | --- | --- |
| `splash--en-light-comfortable--md3-v153.png` | Splash: "Released 29 September 2026" | `en-light-comfortable` |
| `splash--yue_HK-light-comfortable--md3-v153.png` | Splash: "2026年9月29日發佈" | `yue_HK-light-comfortable` |
| `splash--bilingual_en_yue_HK-light-comfortable--md3-v153.png` | Splash: both lines, English first | `bilingual_en_yue_HK-light-comfortable` |
| `dialog-check-for-update--bilingual_en_yue_HK-light-comfortable--md3-v153.png` | Newest-version message (after CJ-020): the English whole, the Cantonese below | `bilingual_en_yue_HK-light-comfortable` |
| `dialog-smart-home--bilingual_en_yue_HK-light-comfortable--md3-v153.png` | Smart home: the search hint reads "Search speakers and lights · 搵喇叭同燈"; the kit buttons are still English only, fixed in `00b14ca67` | `bilingual_en_yue_HK-light-comfortable` |

`probe/language-audit--prepare--md3-v153.json` and `probe/language-audit--preferences-general--md3-v153.json`
were written by `scripts/md3/language-audit.py` in bilingual mode: no English-only label with a
Cantonese translation on either surface (`english_only` 0 in both; labels whose pair did not fit carry
the Cantonese in their tooltip).

A first Smart home capture on `md3-v153` showed the "Printer access codes are credentials" paragraph
in English only while its probe record held both languages; a second run showed both. The first
capture was taken while the auto-wrapping label was still being repainted after its wrap, so it is
not kept as evidence of a defect.

## Release md3-v154 captures (2026-09-29)

Taken by `scripts/md3/sweep-dialogs.py` from the unmodified `md3-v154` release package (the Cantonese
Retraction test with `--po bbl/i18n/yue_HK/BambuStudio_yue_HK.po`, which finds the menu item by its
Cantonese label). Every import of its executables and DLLs resolves in the package or in Windows.

| Field | Value |
| --- | --- |
| Source commit | `00b14ca67` (release `md3-v154`) |
| Package | `BambuStudioMD3-2.8.4153-full.nupkg`, 732,143,762 bytes, SHA-1 `8ac42b12ec13fd2e814690414882e9926cdd0a3f` as listed in `RELEASES` |
| Executable | `bambu-studio.exe` sha256 `04397945b2c9eb671d9c51c5417ab2059d396cffaba0079e393cfed4038ce14e`, `BambuStudio.dll` sha256 `a9ff0c54983d0d9c767301c4787ead30648d605c9993fce7a2db27c7a3165f33` |
| Capture route | hidden Win32 desktop, `PrintWindow` of the dialog, real GPU driver |
| Display scale | 100% |

| File | Dialog | Tuple |
| --- | --- | --- |
| `dialog-keyboard-shortcuts--bilingual_en_yue_HK-light-comfortable--md3-v154.png` | Keyboard Shortcuts (after CJ-016): every label and description inside the dialog | `bilingual_en_yue_HK-light-comfortable` |
| `dialog-check-for-update--bilingual_en_yue_HK-light-comfortable--md3-v154.png` | Newest-version message: the kit button reads "OK · 確定" again | `bilingual_en_yue_HK-light-comfortable` |
| `dialog-smart-home--bilingual_en_yue_HK-light-comfortable--md3-v154.png` | Smart home: "Close · 關閉", bilingual search hint | `bilingual_en_yue_HK-light-comfortable` |
| `dialog-retraction-test--yue_HK-light-comfortable--md3-v154.png` | Retraction test (after CJ-019): "0 mm", "2 mm", "0.1 mm/mm" | `yue_HK-light-comfortable` |
| `dialog-retraction-test--en-light-comfortable--md3-v154.png` | Retraction test in English: the step's unit after the number | `en-light-comfortable` |
| `dialog-temperature--bilingual_en_yue_HK-light-comfortable--md3-v154.png` | Temperature calibration (before CJ-025 and the unit fix of `32a36b134`): units read "Â°C", the header "SETTINGS · 設" | `bilingual_en_yue_HK-light-comfortable` |

A first Smart home capture on `md3-v154` caught its search field mid-repaint (no magnifier, no
outline) and a second run showed it whole, like the `md3-v153` paragraph case above; the sweep now
waits 6.5 s after a dialog opens, two passes of the bilingual decorator, instead of 4.5 s.

## Release md3-v155 captures (2026-09-29)

Taken by `scripts/md3/capture-tuple.py` from the unmodified `md3-v155` release package, with a
profile under `C:\Users\Public\bbsdd`. Every import of its executables and DLLs resolves in the package or
in Windows.

| Field | Value |
| --- | --- |
| Source commit | `c7309b889` (release `md3-v155`) |
| Package | `BambuStudioMD3-2.8.4155-full.nupkg`, 732,154,934 bytes, SHA-1 `00332a4f71ff225d1efc073bb3fb2b9243778bc1` as listed in `RELEASES` |
| Executable | `bambu-studio.exe` sha256 `1fcbcdc7b7f1e723629b07a82a70fed30c2d770aa5837d007fcb40bf83e2dc89`, `BambuStudio.dll` sha256 `9dec027dae933b7a455c3e0d60c3d940ae791a26fe34f7635bb6c99b8fd4e3f3` |
| Capture route | hidden Win32 desktop, `PrintWindow` of the dialog, real GPU driver |
| Display scale | 100% |

| File | Page | Tuple |
| --- | --- | --- |
| `preferences-general--bilingual_en_yue_HK-light-comfortable--md3-v155.png` | Preferences > General (before the second CJ-021 fix): the page still scrolls sideways and the Language list and sliders are cut at the right edge | `bilingual_en_yue_HK-light-comfortable` |
| `preferences-3d--bilingual_en_yue_HK-light-comfortable--md3-v155.png` | Preferences > 3D (before CJ-026): each description's Cantonese line is drawn under the next row's title | `bilingual_en_yue_HK-light-comfortable` |

The General capture lacks the "Reset all warning dialogs" button, which the layout dump taken right
after places at x = 12; the 3D capture of the same run shows it, so the General capture caught a
repaint.

## Layout-probe dumps

`probe/<tuple>--<before|after>.jsonl`, one per main-frame idle dump plus one per opened dialog,
read with `node ui-md3/tests/layout-probe-report.mjs`. The findings for each run are recorded in
`docs/features/design-system/clipping-inventory.md`.

## Limitations

- Display scale tuples (125%, 150%, 200%) need a display at that scale. This host has one display
  at 100%, and per-monitor scaling is a user-visible setting that a hidden-desktop run must not
  change. Those tuples are recorded as not run until a display at each scale is available.
- WebView2 panes are captured with headless Edge per HANDOFF §4, not with `PrintWindow`.
