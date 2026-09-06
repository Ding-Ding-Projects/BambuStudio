# Layout clipping inventory

Every layout clipping defect found on the Windows desktop app, with its tuple, cause, fix and the
evidence that it is gone. This file is hand-written and machine-checked by
`ui-md3/tests/cheap-jor-inventory.test.mjs`: every row needs an id, a surface, the tuple it was
seen at, a symptom, a root cause, a fix commit that exists in this repository, and a status. A row
may only say `verified` when both its before and after captures exist under
`docs/screenshots/md3-everything/` and were taken from the real built artifact.

Statuses:

| Status | Meaning |
| --- | --- |
| `fixed-unverified` | the source fix is committed; the runtime capture pair does not exist yet |
| `verified` | before and after captures exist from the built artifact at the stated tuple |
| `open` | found by the probe or by eye, not fixed yet; the row names the blocker |

## Rows

<!-- cheap-jor-inventory:begin -->
| Id | Surface | Tuple | Symptom | Root cause | Fix commit | Before | After | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| CJ-001 | Preferences dialog | 150% and 200%, any language, any theme | dialog opened at a 100% pixel size and clipped its lower rows | `SetSize(wxSize(780, 580))` without `FromDIP` | 44ed39a18 | pending | pending | fixed-unverified |
| CJ-002 | Plugin download dialog (GUI_App path) | 150% and 200% | dialog sized for 100% on the GUI_App path while the MainFrame path scaled it | unscaled `SetSize(270, 158)` in `GUI_App.cpp` | 44ed39a18 | pending | pending | fixed-unverified |
| CJ-003 | Device monitor base panel | 125% and above | printer-name label truncated at an arbitrary position; panel and label columns fixed at 100% widths | three ellipsize styles ORed on one label; `600x400` and label column widths unscaled | 44ed39a18 | pending | pending | fixed-unverified |
| CJ-004 | Monitor add-machine button, PartSkip label, object-table page field, Tab button, StatusPanel day counter, AMS setting, Create presets, Unsaved changes | 150% and 200% | controls sized in raw pixels clipped their own label at high scale | literal `wxSize(N, M)` in `SetMinSize` / `SetMaxSize` | 44ed39a18 | pending | pending | fixed-unverified |
| CJ-005 | Every wxBoxSizer row (Print pill, Process title precedent) | every tuple (class-level) | over-subscribed rows starve later items to zero width; nothing overflows so nothing is visible | wx pays proportion-0 items in full and hands later items the remainder | 49a505a67 | pending | pending | open |
<!-- cheap-jor-inventory:end -->

CJ-005 is the class the runtime layout probe exists for. It stays `open` until the tuple matrix has
run on the built artifact and every `starved`, `oversubscribed`, `text_clipped` and
`clipped_by_parent` finding either has its own row above or is proven absent by a committed dump
under `docs/screenshots/md3-everything/probe/`.

## Tuple matrix

The matrix is 4 scales x 3 languages x 2 themes x 2 densities = 48 tuples per surface. It is run
by the headless driver against `install-dir/bambu-studio.exe`; the dumps are read with
`node ui-md3/tests/layout-probe-report.mjs`. A run that has not happened is recorded as not run,
never as clean.

| Run | Artifact commit | Scales | Languages | Themes | Densities | Findings | Dumps |
| --- | --- | --- | --- | --- | --- | --- | --- |
| baseline | not run | | | | | | |
| after | not run | | | | | | |

## Suggested articles

- [Runtime layout probe](layout-probe.md)
- [Kit widgets added in the every-element sweep](kit-widgets-2026-09.md)
- [MD3 parity register](md3-parity-register.md)
