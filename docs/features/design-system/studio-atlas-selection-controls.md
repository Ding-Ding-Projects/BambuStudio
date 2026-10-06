# Studio Atlas numeric and selection controls

[繁體中文（香港）](studio-atlas-selection-controls.yue_HK.md)

This bounded source update covers `SpinInput`, `CheckBox`, and `SwitchButton`.
It builds on the Studio Atlas roles and anatomy in
[`design/workflow-refresh.md`](../../../design/workflow-refresh.md), using the
checked-in native design kit. The reference baseline is
`8ce0ad44698d59ccf942518a56645b37d3f9b9eb`.

Material Designer's live creation/export flow and the native application cannot
be launched within this task. This is source implementation with focused checks;
native compilation, rendered acceptance, accessibility interaction and visual
parity remain unverified. The 56 structural boards are design references, not
screenshots, and none of the 1,204 feature obligations is advanced by this change.

## Anatomy and existing behavior

| Family | Appearance and layout | Preserved contract |
| --- | --- | --- |
| Numeric input | Low container field, outline at rest, primary focus/hover border, density corner radius, separated left stepper column, measured editor and trailing unit | Integer parsing, ranges, validation, keyboard arrows, wheel handler, repeat timing, capture release, text and spin events, existing tooltip routing |
| Checkbox | Four-DIP rounded square at the 20-DIP reference size, centered 16-DIP check glyph, existing mixed-state bar and fallback stroke | Native 20-DIP control footprint, boolean and half-checked state, disabled/focus bitmaps, context accent, theme refresh, caller background override, existing reduced-motion selection emphasis |
| Labelled switch | Equally sized inset segments, density padding and rounded rectangle shape, measured caller font, selected primary/on-primary pair | Two labels and their order, boolean state, caller color overrides, maximum-width treatment, bilingual fitting and complete tooltip notes, native toggle event |
| Icon switch | Low-container off track transitioning to the existing primary on track | Existing 44 by 24 DIP track, thumb path, disabled treatment, 150 ms motion, reduced-motion snap and timer ownership |

The numeric editor reserves at least one measured digit plus two DIP. Its width
can grow beyond a too-small caller request to keep the steppers, editor and unit
disjoint. Existing longer numeric strings continue to use native editor scrolling.
Comfortable stepper width/padding are 20/4 DIP; compact values are 18/3 DIP. The
minimum height follows the actual editor or unit font plus vertical padding.
Explicit caller corner radii remain authoritative after a rescale.

The labelled switch adds two-DIP thumb insets. Horizontal padding per segment is
half the density padding, eight or five DIP. Thumb height is the larger of measured
text plus six DIP and density row height minus eight DIP. The outer/inner radii
follow the existing small radius and inset. An unset maximum width is no longer
interpreted as a negative bitmap width. Existing explicit maximum-width font
scaling remains; this update makes no readability claim for arbitrarily small
caller maxima.

No new product strings, translation keys, timers, state engines or callbacks are
introduced. Header APIs do not change. All other classes colocated in
`SwitchButton.cpp`, including `SwitchBoard` and `MultiSwitchButton`, are unchanged.

## Verification and reproduction

Run from the repository root:

```powershell
node --test tests/native_controls/atlas_selection_anatomy.test.mjs
node tests/native_controls/atlas_selection_anatomy.test.mjs --extract "$env:TEMP/atlas-selection-geometry"
```

The six source tests protect 40 exact baseline method bodies, the entire unrelated
class suffix, font measurement, minimum geometry, caller overrides, bilingual
routing and native state variants. Deliberate mutations to range storage, sibling
content, editor measurement and minimum-width reporting must fail their checks.

From the supported MSVC developer command prompt, compile only the extracted
production geometry and focused test, with temporary output:

```bat
cl /nologo /EHsc /std:c++17 /I"%TEMP%\atlas-selection-geometry" tests\native_controls\atlas_selection_geometry_tests.cpp /Fo"%TEMP%\atlas-selection-geometry\geometry.obj" /Fe"%TEMP%\atlas-selection-geometry\geometry.exe"
"%TEMP%\atlas-selection-geometry\geometry.exe"
```

The geometry test passed 6,144 assertions across both densities, 100%, 125%, 150%
and 200% scales, requested widths from zero to 800 DIP, four heights, short/long
units and shrinking minimum-width transitions. It compiles the actual extracted
numeric layout function; it does not compile wxWidgets or simulate rendered text.

The existing `ui-md3/tests/context-menus.test.mjs` suite reports five passes and
one unrelated failure: `CameraHUD.cpp` still calls `m_zoom_percent->PopupMenu`.
That file is unchanged from the baseline and outside this unit. The text-entry
and numeric-field context-menu checks pass; the broader suite is not reported green.

The completed design receipt checker is:

```powershell
node design/workflow-refresh/check-surface-contracts.mjs
```

Its pinned combined-source receipts are separate from this new control unit and
do not prove the new controls have rendered successfully.

## Remaining caller and runtime work

- Validate narrow numeric callers in object-color, Prepare, export and bulk
  dialogs against the new measured minimum. Container resizing remains caller-owned.
- Validate labelled switches in the Params header, Preferences and camera panel
  at their actual maximum widths, all three language modes and all display scales.
- Checkbox labels and large clickable row targets remain caller-owned. This change
  neither enlarges the checkbox footprint nor adds a field label/support stack.
- `LabeledCheckBox`, `ImageSwitchButton`, `SwitchBoard`, custom toggles and other
  colocated controls require their own bounded review.
- Native focus announcements, keyboard traversal, disabled rendering, live density
  changes, theme changes and screen-reader behavior need built-application proof.
- Run the existing normal/minimum viewport, theme, language and scale capture
  matrix when launches are authorized. No capture or parity result exists here.

## Reversibility

This unit mixes appearance and minimum-size/layout behavior, so it is not a
palette-only rollback. If the maintainer dislikes the design, review a revert of
this isolated unit together with its checks and article pair. Preserve unrelated
build, AMS, Print, navigation and data fixes. Reversal is a reviewed source change,
never automatic rollback and never a reset to the historical design baseline.
