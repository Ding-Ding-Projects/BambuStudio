# Studio Atlas native monitor surfaces

This source-only appearance update applies the Studio Atlas hierarchy to native monitoring and printer selection. It does not establish native compilation, rendered geometry, camera operation, or successful printer commands. Application launch and hardware interaction were excluded from this implementation lane.

## Changed surfaces

| Source | Appearance change | Preserved boundary |
| --- | --- | --- |
| `StatusPanel.cpp` | Surface page surround, lowest-container telemetry cards, low-container headers, visible 16-pixel Control and Printing Progress headings, measured progress-header minimum height, single DPI conversion on rescale | Existing camera, temperature, fan, movement, AMS, printing progress, overflow actions and enable rules |
| `MultiMachinePage.cpp` | Measured printer-picker row height, opaque rounded row surface, actual selected and hover fills, contrasting selected text, inset keyboard focus ring | Existing checkbox, row identity, selection events, keyboard activation and device filtering |
| `MediaPlayCtrl.cpp` | Tonal camera footer, square icon target within the existing 40-DIP strip, 13-pixel status and monospaced diagnostic text, consistent horizontal spacing | All playback, session tracking, retry, URL, thread, clipboard and network callbacks |
| `PrintOptionsDialog.cpp` | Resolve supporting text and separator colors when used, rather than retaining startup constants; emphasize AI Detections as a section heading | Existing switches, sensitivity choices, capability visibility, printer commands and validation |

The monitor's recoloring traversal follows the new surface roles. User accent resolution continues through the Device scheme. Printing progress retains its numeric and gauge behavior. Row fills change without changing the row's dimensions or using color as its only selection signal; the existing checked glyph and keyboard behavior remain.

No new strings, identifiers, commands, persistence fields, protocol values, or assets are introduced. The AMS model and its independent reading-state fixes are not changed.

## Explicit remaining ownership

`MonitorPage.cpp` is a hosting sizer, not a second telemetry implementation. It remains unchanged to avoid inserting a second perimeter around `StatusPanel`. `AmsWidgets.cpp` implements the tray data model and remains unchanged. This does not complete all nested material surfaces.

Additional native surfaces require their own source and runtime review: `AMSControl`, `AMSPopup`, `AMSSetting`, `AMSRoad`, `FanControlPopup`, `CameraPopup`, `CameraHUD`, `PrinterPartsDialog`, `MachineList`, `LocalTaskManagerPage`, `CloudTaskManagerPage`, `MultiMachineManagerPage`, nozzle-rack controls, temperature editors, and printer-specific dialogs. Product-owned DeviceWeb and other embedded views have separate ownership. Existing disabled/pending/offline rules remain authoritative; this change does not invent new state or claim those flows were exercised.

## Verification

The existing `ui-md3/tests/ams-reading-state.test.mjs` source-derived model suite passed all eight tests. This checks mixed Lite reading identity, retained settings, ordinary units, single-slot HT indexing, idle telemetry, index bounds and native/web canonical-reader usage. It is not compiled native or hardware evidence.

Source review compares event bindings, hardware-command expressions, and the unchanged AMS model with the preserved implementation baseline. `git diff --check` validates whitespace. No full build, application launch, screenshot capture, installer execution, or printer action was performed.

### DPI review corrections

Independent source review found two defects in the initial appearance revision. The camera button passed an already-scaled value to `SetIconButton`, which owns its own conversion. It now receives the literal design value `32`; its four-DIP margin remains converted at the sizer boundary. At 200%, the production sizing expressions yield a 72-by-64 pixel square target and an 80-pixel total footer height including margins, rather than a 136-by-128 target overflowing that footer.

The printing header formerly retained the sizer minimum calculated at construction. The shared `layout_printing_title` helper now reapplies the heading font, invalidates its measurement, calculates the larger of 40 DIP and measured text plus 16 DIP, replaces both sizer and panel minimums, invalidates the panel cache and lays it out. Construction and `msw_rescale` call the same helper; rescale also invalidates and lays out the owning printing panel. Downward scale transitions can shrink the prior minimum again.

`ui-md3/tests/monitor-atlas-dpi.test.mjs` executes sizing expressions and the helper body extracted from production source. Its three tests failed against the initial revision and passed after correction. They cover camera geometry at 100%, 125%, 150%, 200%; text-height and DPI transitions in both directions; cache/font ordering; and the real construction/rescale call sites. Together with the eight AMS model tests, 11 tests pass. These tests model source geometry and lifecycle, not native window rendering.

A separate follow-up corrects the pre-existing Control-header double `FromDIP(PAGE_TITLE_HEIGHT)` in `StatusPanel::msw_rescale`. Construction and rescale share `layout_control_title`: refresh the heading font and measurement, update its scaled padding, clear the stale sizer floor, measure all actual title/action controls, then replace both panel/sizer minima and relayout. The dedicated check first failed on the old double conversion; all four monitor DPI source/model checks pass after repair. Existing title text, overflow controls and callbacks are unchanged. The independently sized overflow icon remains outside this title-height repair.

Before rendered completion, verify the actual monitor, picker, print options and camera footer in English, Cantonese and bilingual modes, light/dark, comfortable/compact and 100%, 125%, 150%, 200% scale at normal and minimum supported sizes. Record measured text/control rectangles, focus/selection states, DPI and theme transitions, unavailable/pending/confirmed telemetry states, source revision, executable hash and genuine captures. Never replace that evidence with this source receipt.

### Printing-title native type repair

The native compiler reported C2664 for both printing-title layout calls. This remained present at `c1149eab941cb179f3333f994111899281627ffb`: `PrintingTaskPanel::m_staticText_printing` is declared `wxStaticText*`, but `layout_printing_title` required `Label*`. The helper now accepts `wxStaticText*`, the actual member contract and base class of `Label`. Its complete measurement body, heading font, cache invalidation, minima, constructor call and DPI call remain unchanged. No cast, member replacement, wrapping change or telemetry change was introduced.

`ui-md3/tests/printing-title-type-compile.test.mjs` extracts the real production member, full helper and both call statements. It includes the actual `Label.hpp` and configured wxWidgets headers and uses MSVC `/Zs` syntax checking, with no stub widget aliases, linking or window creation. Run it in an MSVC developer environment with `--wx-include <wx-include-directory> --wx-setup <configured-setup-directory>`. Adding `--source-revision c1149eab941cb179f3333f994111899281627ffb --expect-mismatch` reproduces compiler exit 2 and C2664 at both calls; the repaired source returns compiler exit 0. The four existing `monitor-atlas-dpi.test.mjs` checks pass. This proves the isolated production type contract, not a complete StatusPanel translation-unit build or runtime rendering.

Off Windows the two wx arguments may be omitted: the same probe is checked with the host C++ compiler (`CXX`, default `c++`) using the installed wx headers named by `wx-config`, and the expected-failure mode counts that compiler's pointer-conversion rejection at both calls. The MSVC run against the Windows wx build remains the Windows proof.
