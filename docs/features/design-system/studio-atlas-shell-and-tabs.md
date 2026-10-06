# Studio Atlas shell and tab callers

This unit applies the shell/tab contract recorded in design revision `3c8fe2708ba03470c1b171e2fce12fcbdfaab0fc`. It changes the native caption painter, project-tab layout, nested tab-book callers and tab-management dialogs. It includes presentation and visual allocation behavior, not just color changes.

## Concrete coverage

| Owner | Changes |
| --- | --- |
| `BBLTopbar.cpp` | Outlined project-identity chip, 20 DIP folder glyph, 32 DIP history and identity surfaces, contextual close feedback, caption text budget that reserves fixed commands first |
| `MainFrame.cpp` | Project-strip separation from surrounding chrome, DPI reapplication of that spacing, project-strip theme refresh |
| `Widgets/TabStrip.cpp` | Tonal selected surface with paired text, visible marker and semibold name; 250 ms selection wash through the shared reduced-motion route; full horizontal title measurement; bounded pinned-item allocation; checked active destination in overflow |
| `Tabbook.cpp` | Density spacing and rounded nested navigation surfaces, measured selected-font minima, contextual selected foreground/background, explicit caller-padding preservation and DPI refresh |
| `Widgets/TabStripDialogs.cpp` | Opaque result cards around existing scrolling lists, density spacing, wrapping empty/summary text, semantic invalid-pattern summary and card DPI refresh |

The caption retains its 46 DIP height, tool identifiers, drag area, maximize/restore behavior and close handler. Project names remain distinct from the existing dirty-state marker in the project tab strip. The title chip uses the remaining space after fixed commands, up to a 240 DIP text budget. When no text fits, its optional chip collapses instead of consuming command space. The full title is retained as the item's help text and in the project-tab model. History information remains the existing real provenance route and content.

No status bar was inferred from the unused `ProgressStatusBar` include in `MainFrame.cpp`. The existing history/provenance chip receives new paint, but this unit does not create another status surface or claim coverage of status owners elsewhere in the application.

## Overflow and identity

Horizontal tabs measure the complete localized name in the selected font. Group chips, pin markers, dirty markers and visible close controls contribute their actual separately scaled widths. Group headers also retain their complete measured horizontal name. The old 240 DIP tab-width cap no longer silently forces shortened labels before overflow is considered.

The local visual allocator gives pinned destinations priority within the available budget, preserving displayed model order. A pinned destination too wide to fit enters the existing overflow menu. It remains pinned, keeps its close protection and retains its stable identifier. The overflow menu checks the active destination, and its control has a tonal state when the active tab is overflowed. This changes visual allocation; it does not change pinning, grouping, model order, hidden-state persistence or activation semantics.

The strip minimum includes search, optional new and overflow targets. Budgets below that supported minimum can show no tabs; the calculation never spends a negative budget. Horizontal row height expands when its actual font measurements need more room. Configured vertical rail widths remain authoritative. Vertical rails still use the existing disclosed shortening and full tooltip when a name exceeds that width; full wrapped vertical labels remain separate work.

## Dialog composition and accessibility boundaries

Group selection, tab search and bulk-close previews keep their existing `ListBox` instances and event bindings. Each list moves into a keyboard-navigation-enabled rounded card. Its requested height is bounded against one third of the current display work area and its own prior requested height. Lists retain their native scrolling and selection. Empty and summary labels use the existing wrapping implementation, including its CJK handling.

Group-name entry, regex builders, pinned inclusion, preview validation, action enablement, Enter/arrow navigation and Escape cancellation remain in their existing handlers. No translated copy, persistence key, feature engine, print operation or data operation was added. Search result row text still follows the shared list widget's existing ellipsis/tooltip presentation. Small-display fitting of the entire dialog, long checkbox labels and all actual focus transitions require native verification.

The shared motion controller owns reduced-motion behavior. Semantic activation is immediate; animation changes only paint. No window-control action, tab selection, close confirmation or pointer target waits for an animation.

## Verification

```powershell
node tests/native_shared_controls/atlas_shell_anatomy.test.mjs --extract "$env:TEMP/BambuStudio-atlas-shell-layout"
```

In an initialized x64 MSVC command prompt:

```bat
cl /nologo /std:c++17 /EHsc /W4 /WX /I"%TEMP%/BambuStudio-atlas-shell-layout" tests/native_shared_controls/atlas_shell_layout_tests.cpp /Fe:"%TEMP%/BambuStudio-atlas-shell-layout/atlas_shell_layout_tests.exe" /Fo:"%TEMP%/BambuStudio-atlas-shell-layout/atlas_shell_layout_tests.obj"
"%TEMP%/BambuStudio-atlas-shell-layout/atlas_shell_layout_tests.exe"
```

The initial appearance unit passed eight source-contract tests with 44 unchanged function/identifier snapshots against `b88acafa95b88884b4e284a0de15d77f7934cfcb`. The overflow-drag repair below brings the source checks to ten and retains 43 unchanged snapshots. Its intentionally changed drag handler is explicitly listed in the fixture and covered by final model-order regressions. The initializer snapshot uses an explicit adjacent-function boundary because its conditional-compilation alternatives are unsuitable for a simple brace counter.

The standalone test compiles the exact extracted production allocation and title-budget helpers. Seven cases passed with 137,673 assertions. Cases cover empty strips, exact fits, reserved overflow space, oversized pinned destinations, tiny budgets, stable identity partitions across both pinned states and 100/125/150/200 percent scales, and title-command budgets. The assertion count includes exhaustive combinations; it is not a count of independent behaviors.

Native wxWidgets compilation and runtime interaction remain unverified. No application launch, installer execution or rendered capture was performed. The English, Cantonese and bilingual light/dark layout matrix at normal and minimum viewport sizes and all four display scales remains required. Material Designer's live flow is unavailable under the no-launch boundary, so checked-in native design references are source guidance rather than runtime evidence.

### Overflow drag correction

The first bounded overflow allocator exposed a stale-rectangle assumption in `OnTabDragEnd`: it counted every model-displayed tab, including width-hidden controls, then mapped the resulting slot through that unfiltered list. With pinned widths A=100, B=400, C=100 and D=100, a wide-to-narrow resize leaves B's old center at 304 while visible C and D move to 154 and 258. Dropping D at 180, after C, incorrectly moved it before C.

The corrected handler gathers only shown tab controls. It excludes the dragged identity when choosing the next visible neighbor, resolves that neighbor's stable ID to its current model position, and adjusts the insertion boundary for source removal. A drop after the final visible neighbor anchors there rather than after an unrelated hidden model tail. `MoveTab` and `Model::move` remain unchanged, including pinned-region constraints.

The standalone regression uses the production overflow allocator, production drag-target helper and verbatim extracted `Model::add`, `index_of`, `pinned_count` and `move` bodies. Its minimal storage adapter replaces wxString title storage with std::string; it does not run wxWidgets or simulate native input. Eight cases passed with 28 assertions on actual resulting tab order, including the reported wide-to-narrow sequence, before/after neighbors, unchanged-position drops, no visible neighbors, a hidden tail and pinned-region protection. Removing the final-neighbor boundary offset in the temporary extracted helper produced two failed assertions and exit 1. Restoring the production helper returned all 28 assertions to passing. The ten source checks also reject removing the shown-control filter.

After the extraction command above, compile and run the additional focused regression:

```bat
cl /nologo /std:c++17 /EHsc /W4 /WX /I"%TEMP%/BambuStudio-atlas-shell-layout" tests/native_shared_controls/atlas_tab_drag_tests.cpp /Fe:"%TEMP%/BambuStudio-atlas-shell-layout/atlas_tab_drag_tests.exe" /Fo:"%TEMP%/BambuStudio-atlas-shell-layout/atlas_tab_drag_tests.obj"
"%TEMP%/BambuStudio-atlas-shell-layout/atlas_tab_drag_tests.exe"
```

## Reversal and remaining work

Revert this unit separately to restore the previous shell paint and visual allocation. Such a revert also restores the old forced-visible pinned overflow behavior and horizontal title cap. It is therefore a mixed presentation/layout reversal, not a paint-only reversal. It does not require reverting workflow-page integration, field geometry or the neutral role palette.

Remaining work includes full wrapped vertical tab labels, native dialog fitting at extreme display/font settings, actual title-toolbar keyboard focus evidence, unrelated status owners, and all other inventoried application surfaces. Shared component uptake does not prove complete application coverage. This source unit does not complete the application-wide redesign.
