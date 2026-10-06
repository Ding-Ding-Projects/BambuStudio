# Studio Atlas shared list rows

This bounded unit changes `Widgets/ListBox.cpp/.hpp`, the shared owner-drawn `wxVListBox`. It follows the existing Studio Atlas density, surface, shape and motion roles. It does not change caller dimensions, row identity, selection, check state, event contracts or persistence.

## Reachable callers and size ownership

| Caller | Existing route and constraint |
| --- | --- |
| `WorkspacePanel.cpp` | Checklist page, checkable rows, 160 DIP minimum height, expanding layout. `wxEVT_CHECKLISTBOX` indexes the workspace checklist and reads `IsChecked`. |
| `Appearance/AppearanceEditorPopover.cpp` | Font-family and appearance-preset search results, two 132 DIP requested-height lists. Existing visible-index maps resolve selection; preset double-click applies the selected preset. |
| `Schedule/ScheduledSettingsPanel.cpp` | Schedule rule list, 180 DIP requested height; selection refreshes status and double-click edits. |
| `SmartHomeDialog.cpp` | Speaker/light result list, 150 DIP requested height and existing filtered rows. |
| `SettingsDraftPanel.cpp` | Page/draft picker list expands inside the existing 520 by 440 DIP dialog; search maps visible rows to stable choices. |
| `Widgets/TabStripDialogs.cpp` | Group, tab-search and bulk-close result lists. Existing requests are 360 by 220, 380 by 180 and 460 by 240 DIP. The existing result-card helper bounds list height against one third of the display work area. |

These are nine construction sites across six caller files. No caller file changes in this unit. Native tree/data-view lists, combo-box dropdowns and other list owners do not inherit this implementation and remain separately inventoried.

## Row anatomy and states

Uncustomized lists use the existing comfortable 14 or compact 13 body font. Each row measures its actual current font and complete multiline string. Its height is at least the density's 40 or 32 DIP floor, and grows for taller text or the 20 DIP check glyph plus vertical padding. Explicit caller fonts remain authoritative through `Rescale`; `SetFont` refreshes cached row measurements. DPI changes refresh the rows without assigning a new control size.

The rounded row pane has 4 DIP horizontal and 2 DIP vertical insets, with the density's 10 or 8 DIP small radius. Text retains its existing 12 DIP inner padding. Check glyphs retain their 20 DIP size and 8 DIP trailing gap. Their left origin remains 16 DIP, and the existing click boundary remains 36 DIP. Every dimension is converted to pixels once. Width allocation clamps inside the row pane, including a zero-width result when a narrow caller leaves no text space. Empty text rectangles never reach ellipsizing. Glyph drawing is clipped to the actual row rectangle.

Selected rows pair `SecondaryContainer` and `OnSecondaryContainer`, including the check glyph. Disabled selection uses `SurfaceContainerLow` with `OnSurfaceVariant` text/glyphs. Resting unselected rows preserve the control background. Hover uses `SurfaceContainerHigh`; a selected hovered row receives an eight-percent `OnSecondaryContainer` state layer. Focused selected rows receive a 2 DIP `Primary` ring inset inside the rounded pane. Focus gain and loss refresh paint without changing selection.

Hover state updates immediately and paint transitions over the shared 100 ms `short2` duration. The animation is bound to the owning list and stops during destruction, row replacement, clearing or rescaling. Reduced motion paints the terminal state immediately through the existing shared route. Animation callbacks only refresh valid rows; they never select, check, activate or dispatch caller commands.

Long text keeps the existing end ellipsis and complete tooltip. No new labels, translation keys, callback engine or data operation is introduced. Existing native arrow/Home/End/Page navigation, checklist Space handling, glyph clicks, selection and double-click events, and custom scrollbar ownership remain unchanged. This unit does not establish a new keyboard-only full-label disclosure or replace the list's native accessibility implementation.

## Focused verification

```powershell
node tests/native_shared_controls/atlas_listbox_anatomy.test.mjs --extract "$env:TEMP/BambuStudio-atlas-listbox"
```

Nine source checks cover caller reachability, measured fonts, density and DPI lifecycle, preserved checkbox dimensions, selected/focus state pairing, bounded drawing, owner-bound reduced-motion feedback and 17 baseline behavior bodies from `50715f4355e8b845042bd4809bac2da2ce5c40f4`. The row replacement and clear bodies differ only by resetting visual hover state. Negative source checks reject fixed-height text measurement and a changed checkbox padding constant.

The standalone C++ fixture compiles the exact extracted production geometry helpers in an initialized x64 MSVC command prompt:

```bat
cl /nologo /std:c++17 /EHsc /W4 /WX /I"%TEMP%/BambuStudio-atlas-listbox" tests/native_shared_controls/atlas_listbox_geometry_tests.cpp /Fe:"%TEMP%/BambuStudio-atlas-listbox/atlas_listbox_geometry_tests.exe" /Fo:"%TEMP%/BambuStudio-atlas-listbox/atlas_listbox_geometry_tests.obj"
"%TEMP%/BambuStudio-atlas-listbox/atlas_listbox_geometry_tests.exe"
```

Four geometry cases passed with 24,088 assertions: normal text/check origins; larger or multiline font row floors; both densities across 100/125/150/200 percent scales and widths 0 through 500 pixels; and degenerate dimensions. The assertion count includes exhaustive combinations, not that many independent behaviors. Removing the check-width clamp from the temporary extracted helper produced 723 failed assertions and exit 1. Restoring the production helper passed all 24,088 assertions.

No native GUI target was compiled or launched. Actual selection/check interaction, focus pixels, tooltip delivery, scrollbar behavior and the English/Cantonese/bilingual light/dark layout matrix remain unverified. No installer, hardware operation or rendered capture ran. The checked-in Atlas design contract is source guidance under the no-launch boundary, not rendered evidence.

## Reversal and remaining limits

This is a separate shared-row appearance and measured-layout unit. Reverting it restores the previous fixed row-height behavior, 13 body-font default, unbounded narrow text rectangle and row state paint. It does not require reverting caller cards, selection models or the neutral palette.

Caller-size changes, new persistent supporting labels, keyboard-only full-text disclosure and other list implementations remain outside this slice. Nine reachable construction sites establish source coverage, not runtime parity or completion of the application-wide redesign.
