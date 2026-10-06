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

Before rendered completion, verify the actual monitor, picker, print options and camera footer in English, Cantonese and bilingual modes, light/dark, comfortable/compact and 100%, 125%, 150%, 200% scale at normal and minimum supported sizes. Record measured text/control rectangles, focus/selection states, DPI and theme transitions, unavailable/pending/confirmed telemetry states, source revision, executable hash and genuine captures. Never replace that evidence with this source receipt.
