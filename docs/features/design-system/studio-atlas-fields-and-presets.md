# Studio Atlas fields and preset controls

This appearance unit implements the existing field and preset composition from the Studio Atlas design contract. It follows the shared control paint changes and the porcelain/slate role palette. It does not complete the application-wide redesign.

## Changed surfaces

| Source | Concrete coverage |
| --- | --- |
| `Widgets/TextInput.cpp` | Filled field backing, density radius and minimum row height, inset keyboard focus outline, measured editor/prefix/unit/label/support composition, readable existing support text, paired validation foreground and background |
| `Widgets/ComboBox.cpp` | Read-only selection face, hover/pressed/focus surfaces, matching contextual focus foreground, remeasurement after hiding the editor and setting the label font |
| `BitmapComboBox.cpp` | Windows owner-drawn item backing and rounded selection surface inside the existing native item rectangle |
| `Field.cpp` | Single-line option field outer minimum, multi-variant prefix padding and row gap, preservation of the measured minimum when restoring variant widths after DPI changes |
| `Tab.cpp` | Preset-header density spacing and minimum row height, refreshed from logical metrics during creation, rescale and theme updates |

The field geometry measures each string with the font used to paint it. The support line is below its existing label at the same horizontal position. Prefix and unit use the editor font. A single calculation reserves each part once and provides the minimum required width. Editor width never becomes the native `-1` default-size sentinel when a parent allocates too little space. A forced allocation below the measured minimum is contained by the paint clip, but such an allocation still requires repair in its owning parent layout. The minimum-width calculation is not evidence that every parent honors it.

Comfortable and compact fields request the existing 40/32 DIP row floor and 10/8 DIP small radius, increasing height when the measured label/support stack or icon needs more room. Insets derive from the active padding metrics; intra-field spacing is 8 DIP. Geometry enters the layout calculation in device pixels, with no second DPI conversion. Font, prefix, label, icon, density and same-size updates remeasure and reposition the editor. An explicitly supplied corner radius remains authoritative.

The header remains the existing panel and sizer. Its preset choice, search control, save, delete, export and rollback controls retain their identities and event routes. The bitmap choice continues to use the native drawing path for text and bitmaps. This change does not replace its item-height calculation, item data or keyboard handling.

## State and behavior boundaries

Read-only choices use a low container at rest, high container on hover and highest container while pressed. Focus uses the contextual secondary container and its paired foreground. Disabled takes precedence. The existing shared state transition machinery remains responsible for feedback and reduced-motion handling; this unit adds no timer and never delays an event for animation.

Text validation retains the existing checkers, tooltip and optional dialog. Invalid entry text receives the matching foreground for the error container. Successful validation restores the caller's current state colors rather than a hardcoded neutral fill. No new validation engine, message, persistence key, preset inheritance behavior, search identifier or settings registration is introduced.

## Remaining caller work

- Persistent option-name labels, reset and inherited-value markers, separate validation rows, narrow stacked layouts and category card header/footer composition still belong to the option-row and panel callers. This unit uses only the support text already provided to `TextInput`.
- `OptionsGroup`, `ParamsPanel`, `PresetComboBoxes` and `GUI_ObjectList` are separately owned compositions. Their local styling, explicit sizing and overrides require their own verification.
- Custom controls such as `SpinInput`, multiline `TextAreaEditor`, slider tracks, color-data swatches and specialized parameter editors have independent paint or layout paths. The enclosing field primitive does not prove complete coverage of them.
- `BitmapComboBox` changes here affect its Windows owner-drawn background path. The separate macOS item renderer and GTK-native behavior are not restyled or verified by this unit.
- Read-only choices with exceptionally long labels or support text can request a wider minimum. Parent scrolling, wrapping and minimum-window behavior must be checked in the built application.
- Validation presentation remains the existing tooltip/dialog route. A persistent inline validation row is not implemented by these changes.

## Focused verification

Run the source-contract checks with:

```powershell
node tests/native_shared_controls/atlas_field_anatomy.test.mjs --extract "$env:TEMP/BambuStudio-atlas-field-layout"
```

In an initialized x64 MSVC command prompt, compile only the standalone geometry test, using the extraction directory as the include directory:

```bat
cl /nologo /std:c++17 /EHsc /W4 /WX /I"%TEMP%/BambuStudio-atlas-field-layout" tests/native_shared_controls/atlas_field_layout_tests.cpp /Fe:"%TEMP%/BambuStudio-atlas-field-layout/atlas_field_layout_tests.exe" /Fo:"%TEMP%/BambuStudio-atlas-field-layout/atlas_field_layout_tests.obj"
"%TEMP%/BambuStudio-atlas-field-layout/atlas_field_layout_tests.exe"
```

The seven source checks passed. They include a deliberate missing-relayout mutation that the lifecycle guard rejects, plus 22 byte-identical behavior-function snapshots against baseline `df300fb9d93991751e840bdc586d2f770b47b9cf`. The compiled production geometry helper passed five cases with 1,428 assertions. Cases cover bare editors, left/right label stacks, read-only choices, undersized allocations, and density/scale combinations with long label widths. Assertion count is not a count of independent behaviors.

This is source and calculation evidence only. The application was not built or launched for this unit. Native wxWidgets compilation, rendering, keyboard interaction and the English/Cantonese/bilingual light/dark matrix at 100%, 125%, 150% and 200% scale remain unverified. Material Designer's live flow is unavailable under the task's no-launch boundary; the checked-in native design contract supplies the implementation reference, not runtime evidence.

## Reversal

This unit can be reverted as a separate appearance/layout commit. It introduces no settings migration or external caller API. Reverting it restores the earlier field geometry, preset spacing and choice paint while retaining navigation, workflow pages and printer behavior from earlier commits. Reverting unrelated navigation or workflow commits is not required. Subsequent caller changes must be reviewed before reversal if they begin relying on these measured minima.

## Preset spacer compiler repair

The native build reported C2039 because wxSizerItem has no SetSpacer member. applyPresetHeaderAnatomy now uses the installed wxSizerItem::SetMinSize(int, int) API. The real header writes m_minSize without replacing the item or its owned spacer; the inspected wx implementation retains that minimum in CalcMin and applies the allocated dimensions to the spacer during SetDimension. Existing parent-sizer ownership, DIP-derived padding, height 1 and density/DPI refresh calls are unchanged.

In an initialized MSVC developer environment, run node tests/native_shared_controls/preset_spacer_compile.test.mjs --wx-root <installed-wx-prefix>. The check extracts the production spacer statement and compiles it against the actual installed wx/sizer.h and matching setup header. With --source-revision c1149eab941cb179f3333f994111899281627ffb --expect-missing-api it reproduces C2039 for SetSpacer; the corrected statement compiles successfully. No substitute wx class or compatibility method is used. The existing focused field/preset density-DPI lifecycle source check also passes.

Off Windows, `--wx-root` may be omitted: the test then compiles the same statement with the host C++ compiler (`CXX`, default `c++`) against the installed wx headers named by `wx-config`, and the expected-failure mode accepts that compiler's missing-member diagnostic for `SetSpacer`. The MSVC run against the Windows wx build remains the Windows proof.

This verifies the repaired API call, not the full Tab.cpp translation unit, linked application, runtime spacer layout or rendered interface. No full build or application launch was performed by this repair lane.
