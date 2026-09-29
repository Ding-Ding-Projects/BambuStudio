# Layout clipping inventory

Every layout clipping defect found on the Windows desktop app, with its tuple, cause, fix and the
evidence that it is gone. This file is hand-written and machine-checked by
`ui-md3/tests/clipping-inventory.test.mjs`: every row needs an id, a surface, the tuple it was
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

<!-- clipping-inventory:begin -->
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
| CJ-011 | Every kit SearchField (Prepare sidebar, Preferences, Config profiles, Version history, ...) | every tuple | the regex-mode and builder buttons paint over the pill outline above and below themselves, and an empty 44 px slot sits at the trailing edge | 44 px icon buttons inside a 44 px pill cover its 1 px outline; the Clear button's slot was reserved permanently even while hidden | baabd4e17 | prepare--en-light-comfortable--after.png | prepare-advanced--en-light-comfortable--after.png | verified |
| CJ-012 | Prepare sidebar with Advanced settings open (every row: printer card, ink pills, search pills, process tab strip) | every tuple (layout probe, 1200 x 800) | every sidebar row is laid out 1271 px wide inside a 479 px scroller and cut at the sidebar edge; a horizontal scrollbar takes 17 px of height; the process tab strip ends at "Otl" | the reparented ParamsPanel header sizer put the title at proportion 1 (56 px min) beside stretch spacers of 2, 1 and 12; wxBoxSizer::CalcMin scales that minimum by the total proportion (56 x 16 + fixed = 1271) and update_sidebar_scroll_body honours the content minimum as the virtual width | 3f4d8ffeb | prepare-advanced--en-light-comfortable--before.png | prepare-advanced--en-light-comfortable--after.png | verified |
| CJ-013 | Prepare sidebar, Process settings tree (category pill strip and page area) | every tuple (layout probe, 1200 x 800, default sidebar width) | the category strip hid Speed/Support/Others behind an overflow (the Others pill starved to 16 px) and the settings tree scrolled inside a 144 px inner scroller, so the user had to drag the sidebar bigger to see any setting | TabCtrl's pill mode used the flat strip's hide-what-does-not-fit layout, and the ParamsPanel was a proportion-3 item with a 240 px floor inside the scrolling sidebar body, so its page view got only the leftover height | 92cd7bce7 | prepare-tree-categories--en-light-comfortable--before.png | prepare-tree-categories--en-light-comfortable--after.png | verified |
| CJ-014 | Message dialogs with a long action label (nozzle-diameter choice, custom `SetButtonLabel` actions, "Do not execute", "Go to ...") | every tuple (class-level) | the action buttons are drawn cut short at the 44 DIP floor: "Left..." and "Rig..." instead of "Left nozzle: 0.4mm" and "Right nozzle: 0.6mm" | `MsgDialog::add_button` let every footer button shrink (`SetAllowShrink(true)`), so the kit Button reported a 44 DIP minimum and the footer's flex grid gave each action exactly that | 9615c9418 | dialog-check-for-update--bilingual_en_yue_HK-light-comfortable--md3-v143.png | dialog-check-for-update--bilingual_en_yue_HK-light-comfortable--before.png | verified |
| CJ-015 | Every kit search field (Smart home, Config profiles & backup, Version history, the Prepare sidebar, Preferences) | every tuple (class-level) | the pill's rounded right end is covered: its outline stops short and a sliver of the end arc floats beside the last icon button | the trailing 40 px icon button is a child window that paints its whole square, and at 5 px of trailing padding it covered the arc of the 22 px radius end | d27eadfdb | dialog-smart-home--en-light-comfortable--before.png | dialog-smart-home--en-light-comfortable--md3-v150.png | verified |
| CJ-016 | Keyboard Shortcuts, section list and shortcut descriptions (bilingual) | bilingual_en_yue_HK-light-comfortable | compact labels run past the scrolled panel: "Objects list · 物件清" and a description cut at the dialog edge | the bilingual decorator measured the fit against the containing sizer, then against the visible width plus the room the dialog may grow by (`d27eadfdb`, still cut on `md3-v150`); the section list is a fixed-width panel and the descriptions sit in a scrolling page, so neither grows with the dialog. This commit takes a pair back to English once the settled layout does not fully show it | 00b14ca67 | dialog-keyboard-shortcuts--bilingual_en_yue_HK-light-comfortable--before.png | pending | fixed-unverified |
| CJ-017 | Config profiles & backup, profile list | every tuple (worst in bilingual) | the list shows one row in English and half a row cut through its middle in bilingual mode | a data view asks for almost no height, so the list got only what the text above it left over in a fixed 720 x 700 dialog | 9670a437a | dialog-config-profiles-backup--bilingual_en_yue_HK-light-comfortable--md3-v143.png | dialog-config-profiles-backup--bilingual_en_yue_HK-light-comfortable--md3-v150.png | verified |
| CJ-018 | Temperature calibration, settings labels (bilingual) | bilingual_en_yue_HK-light-comfortable | "Start temp: · 開", "End temp: · 結束", "Temp step: · 溫度": the Cantonese is cut at the label edge | the labels are created 120 px wide, which becomes their minimum, and the decorator counted the sizer's slack as room they could grow into | efaa98db2 | dialog-temperature--bilingual_en_yue_HK-light-comfortable--before.png | dialog-temperature--bilingual_en_yue_HK-light-comfortable--md3-v150.png | verified |
| CJ-019 | Retraction test, step field (every centred calibration and preset field with a unit) | yue_HK-light-comfortable | "0.1" is cut and the first "mm" of "mm/mm" hides behind the entry ("5 /秒" for "5 mm³/s" in Max volumetric speed); in English the number has 26 px, so any value longer than "0.1" is cut too | a centred field drew its unit at its left edge, under the entry, and the field is created 90 px wide with no minimum left for the number (`a849963bf` keeps room for the number; this commit draws the unit after it) | f3aab6af1 | dialog-retraction-test--yue_HK-light-comfortable--before.png | pending | fixed-unverified |
| CJ-020 | Every message dialog body (bilingual): the newest-version notice, and every plain message | bilingual_en_yue_HK-light-comfortable | "This is the newest versio" with a blank strip at the end, and the Cantonese line never shows | the body sits in a scrolling page whose minimum and maximum size were fixed from the one-line English; the bilingual decorator made the label two lines afterwards, the page could not grow, and a vertical scrollbar took the end of the line | 2b8fa5d8b | dialog-check-for-update--bilingual_en_yue_HK-light-comfortable--before.png | pending | fixed-unverified |
| CJ-021 | Preferences, every page (bilingual) | bilingual_en_yue_HK-light-comfortable | the page grows a sideways scrollbar and the controls at the end of each row (the Language list, the Funny level sliders, the switches, Login Region) move out of sight; descriptions such as "No warnings when loading 3MF with modified G-codes · ..." run as one 629 px line under their switch and past the 533 px page | a Preferences page scrolls instead of growing with the dialog, but the bilingual decorator let a label count on the room the dialog may grow by (up to 40 %), and it paired the labels Preferences wraps to 320 DIP on one unwrapped line; a label that stretches along its row also counted the row's spare room twice | 2e80091ef | preferences-general--bilingual_en_yue_HK-light-comfortable--md3-v151.png | pending | fixed-unverified |
<!-- clipping-inventory:end -->

CJ-014, CJ-015, CJ-017 and CJ-018 were verified from released packages, with nothing added to them, on a hidden desktop (2026-09-29): the newest-version message in bilingual mode draws "OK · 確定" whole on `md3-v148` where `md3-v143` drew "OK ·..." (CJ-014); on `md3-v150` the search field draws its full rounded right end (CJ-015), the Config profiles list shows its row whole (CJ-017), and the Temperature calibration labels stay English with the Cantonese in their tooltip instead of cutting it (CJ-018). CJ-016 was still cut on `md3-v150`; its fix is `00b14ca67`.

CJ-013 was verified on attempt 28 (source `92cd7bce7`): the five pills lay out on two rows (Others at y = 52), the page view is 1672 px tall so the sidebar body (content 2549 px in a 645 px client) is the only scroller, and the probe reports no starved row in the sidebar (dump `probe/prepare-tree-categories--en-light-comfortable--attempt28.jsonl`). 

CJ-012 was verified on attempt 23 (`ba6ebba7baad232e` DLL prefix, source `2dcc26658`): the sidebar scroller reports client 479 x 645 and content best 443 x 617 at 1200 x 800, no row wider than the client, no horizontal scrollbar; at the 1000 x 600 frame minimum the scroller is 462 x 445 with a vertical scrollbar and the settings tree keeps its own inner scroller (captures `prepare-advanced-minimum--en-light-comfortable--after.png` and `...-scrolled--after.png`, dumps under `probe/prepare-advanced*-attempt23.jsonl`).

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
