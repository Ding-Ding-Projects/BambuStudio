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
| CJ-005 | Every wxBoxSizer row (Print pill, Process title precedent) | every tuple (class-level) | over-subscribed rows starve later items to zero width; nothing overflows so nothing is visible | wx pays proportion-0 items in full and hands later items the remainder | 49a505a67 | prepare--en-light-comfortable--before.png | prepare--en-light-comfortable--after.png | verified |
| CJ-006 | Device tab placeholder page ("Printer Connection") | every tuple, 1200x800 frame | the heading is cut off at the top of the page and cannot be scrolled into view | body was a flex box centred at exactly 100vh; a taller card overflowed upward where no scrollbar reaches | fbfae7d38 | device--en-light-comfortable--before.png | device--en-light-comfortable--after.png | verified |
| CJ-007 | Prepare action bar, Slice / Print options segments | every tuple | the 24 px options segment shows as a bare green sliver two pixels short of its minimum, no chevron | the segment had no content once the raster caret was dropped; SideButton measured it below the sizer allocation | bb4a0988a | prepare--en-light-comfortable--before.png | prepare--en-light-comfortable--after.png | verified |
| CJ-008 | Prepare ink rows, colour swatch button | every tuple (first after-build capture) | swatch widened to 44 px and painted a second "1" beside the badge | Plater set the swatch label to the filament index as an accessibility hack; the kit Button paints labels | bb4a0988a | evidence/ink-row-swatch-label--bbc9db5cf.png | prepare--en-light-comfortable--after.png | verified |
| CJ-009 | Home tab, window caption bar | every tuple, first show of the 1200 px frame | the caption bar (menus, project chip, window controls) stays 787 px wide on a 1186 px client until a tab switch; the window controls sit mid-window and the rest of the strip is bare | the frame widened the bar on every size event, but the width update ends in update_responsive_title, which calls Realize, and wxAuiToolBar::Realize resizes the bar to its content unless wxAUI_TB_NO_AUTORESIZE is set | 9b012c1e7 | home--en-light-comfortable--before.png | home--en-light-comfortable--after.png | verified |
| CJ-010 | Prepare sidebar, Printer card | every tuple (layout probe) | a stock cog button sits shown at 0,0 with zero width on the card, outside every sizer; invisible to the user, but a live stock control the tab order can reach | PlaterPresetComboBox builds a legacy ScalableButton on its parent panel; the card replaced it with a kit edit button and never hid the original (the Process card already did) | 96a054981 | home--en-light-comfortable--before.png | prepare--en-light-comfortable--after.png | verified |
| CJ-011 | Every kit SearchField (Prepare sidebar, Preferences, Config profiles, Version history, ...) | every tuple | the regex-mode and builder buttons paint over the pill outline above and below themselves, and an empty 44 px slot sits at the trailing edge | 44 px icon buttons inside a 44 px pill cover its 1 px outline; the Clear button's slot was reserved permanently even while hidden | baabd4e17 | prepare--en-light-comfortable--after.png | pending | fixed-unverified |
| CJ-012 | Prepare sidebar with Advanced settings open (every row: printer card, ink pills, search pills, process tab strip) | every tuple (layout probe, 1200 x 800) | every sidebar row is laid out 1271 px wide inside a 479 px scroller and cut at the sidebar edge; a horizontal scrollbar takes 17 px of height; the process tab strip ends at "Otl" | the reparented ParamsPanel header sizer put the title at proportion 1 (56 px min) beside stretch spacers of 2, 1 and 12; wxBoxSizer::CalcMin scales that minimum by the total proportion (56 x 16 + fixed = 1271) and update_sidebar_scroll_body honours the content minimum as the virtual width | 3f4d8ffeb | prepare-advanced--en-light-comfortable--before.png | pending | fixed-unverified |
<!-- cheap-jor-inventory:end -->

CJ-005 is the class the runtime layout probe exists for. It moved to `verified` on the attempt-13
matrix (24 dumps, 12 tuples, Prepare and Preferences): no `zero_sized` and no `text_clipped` finding
remains, and every `starved` (48), `clipped_by_parent` (12) and `oversubscribed` (240) line is one
class, the main frame's sizer minimum (1000 x 951 px, driven by the Prepare sidebar's 900 px minimum
height) on a window laid out at 1200 x 800. Those rows carry the numbers and no visible effect at
this size: the sidebar body scrolls, the frame enforces its own 1000 x 600 minimum, and no control
is drawn short. The commit that landed the matrix said the reader found no clipped-by-parent
window; it found these twelve, all in that one class, and this note is the correction.

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
