# Studio Atlas calibration child surfaces

This bounded visual unit follows the `calibration` surface contract in design
revision `3c8fe2708`. It covers `CalibrationWizardPage.cpp`,
`CalibrationWizardStartPage.cpp`, `CalibrationWizardPresetPage.cpp`,
`CalibrationWizardCaliPage.cpp` and `CalibrationWizardSavePage.cpp`.

## Changes and preserved behavior

| Surface | Visual change | Preserved behavior |
| --- | --- | --- |
| Shared caption and step guide | Density-based caption gap; wrapping actual step labels; readable secondary state and primary-container selected state | Step labels, count, order, selected index and existing rebuild route |
| Shared action group | Wrapping existing actions without proportional stretch spacers; active-density gaps | Action construction order, enablement and exact posted action IDs |
| Start pages | Stronger section headings and subordinate explanatory text | Instructions, illustrations, calibration-method choices and device capability decisions |
| Preset pages | Distinct reading/advice surfaces; stronger calibration-type and printing-parameter headings | Material/nozzle selections, ranges, units, validation, synchronization and printer identities |
| Active calibration | Separation around the real printing panel and before subsequent actions | Telemetry, progress, pause/resume, abort confirmation and hardware callbacks |
| Save/result pages | Stronger result headings, consistent reading surfaces and distinct partial-failure regions | Result association, factor values, naming, save decisions and preset updates |

Neutral changes use existing semantic roles. No localization key, copy, engine,
printer command, callback, timer or animation is added. Existing controls retain
their focus and reduced-motion behavior. Removed step connector decorations are
replaced by the actual ordered labels and selected-state surface, not a fabricated
shorter sequence.

## Source verification

Run `node --test tests/calibration_children_atlas.test.mjs`. Five source checks
compare these files with baseline `34fa40252bbb9b755fd22eb503280a045d8923ee`.
After excluding the explicit paint and sizer-layout statements, non-visual tokens
remain identical. Every literal, localization key, unit and step string remains
identical. Other checks cover step identity and footer composition, with deliberate
action-ID and result-method mutations rejected. The scanner handles indexed
targets and escaped multiline strings. These checks do not compile C++ or prove
runtime behavior.

## Remaining evidence and layout boundaries

No application, browser, installer or printer was launched. No full build,
screenshot, measured native control receipt or runtime acceptance is claimed.
The live design flow remains unavailable under the explicit no-launch boundary.

The existing 1100-DIP calibration page minimum, fixed instructional text widths,
multi-column preset/result grids and scrolling ownership remain unchanged.
Wrapping sizers do not by themselves establish fit at every translated minimum
viewport. The action group remains within the existing page scroll owner; this
unit does not implement a separately pinned footer. Minimum-size, language,
theme, scale, keyboard, focus and real step-transition evidence remain required.
These limitations prevent treating the five-file refresh as completion of the
entire calibration surface contract.

Keep this unit separately reversible from setup-index painting, calibration
algorithms and printer behavior. The parent integration owns build and native
verification receipts.
