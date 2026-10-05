# Surface delivery mapping, 5 October 2026

This records the bounded delivery scope for issues #43, #45 and #51, with the surface requirements shared by #36. The starting revision is `bbfc86ae67211658eb18c528520a77756122de60`. Existing implementations are retained rather than rebuilt. The maintainer selected implementation integration and release delivery as the acceptance criteria for this pass. Tests, lint, type checks, static-analysis suites, runtime interaction, installer execution and screenshots are not run in this pass. This document does not establish runtime correctness or visual parity.

## Language modes, issue #43

| Requirement | Existing source |
| --- | --- |
| Catalogue extraction, context and plural compilation | `scripts/i18n/update_catalogs.py`, `bbl/i18n/po_catalog.py`, `bbl/i18n/compile_catalog.py`, `bbl/i18n/yue_HK/compile_translation.py` |
| English fallback, Cantonese and bilingual lookup | `src/slic3r/GUI/I18N.cpp`, `src/slic3r/GUI/LanguageMode.cpp` |
| Native fit-aware bilingual presentation | `src/slic3r/GUI/BilingualDecorator.cpp`, `src/slic3r/GUI/Widgets/Label.cpp` |
| Self-drawn controls and canvas wording | `src/slic3r/GUI/Widgets/TabStrip.cpp`, `src/slic3r/GUI/Widgets/TextTabbar.cpp`, `src/slic3r/GUI/ImGuiWrapper.cpp` |
| Localized feature articles and changelog | `docs/features/**/*.yue_HK.md`, `resources/changelog/changelog.yue_HK.json`, `src/slic3r/GUI/ChangelogDialog.cpp`, `src/slic3r/GUI/CommandPalette.cpp` |
| Independent website tone ladders | `ui-md3/site/copy.js` |

The known source gap from the responsive reconciliation was the `automation.body` entry: both languages held one prose variant despite the independent tone controls. This delivery adds three distinct variants per language using the catalogue's existing level mapping. All variants retain development status, stdio and authenticated HTTP transport, the project/slicing/configured-printer scope, and separately tracked physical-printer acceptance. The former promise that builds and verification run on GitHub Actions is removed: product capability copy does not promise a particular build environment or imply verification occurred. No validation or rendered interaction was performed.

## Kit controls, issue #45

The kit conversion is present in `src/slic3r/GUI/Widgets`: labels, fields, buttons, pickers, menus, caption shells, tables, scrolled panels, multiline text and HTML views. `MD3Menu.cpp` supplies searchable menus and text context-menu routing, enabled at startup in `GUI_App.cpp`. `MD3DialogChrome.cpp` follows a dialog's changing title; `AMSItem.cpp` synchronizes the load-to-nozzle title. `MD3ScrollBars.cpp`, `MD3ScrolledWindow.cpp`, `MD3DataView.cpp`, `TextArea.cpp` and `MD3HtmlWindow.cpp` provide the kit scrollbar routes. The older comment listing multiline scrollbars as missing is superseded by these current implementations.

The existing article [Native controls on the kit](../features/design-system/native-controls.md) records the remaining intentional native boundaries, including operating-system file dialogs. This pass introduces no additional conversion. Historical source and capture claims remain historical evidence, not results of this pass.

## Preview report, issue #51

| Reported item | Source resolution |
| --- | --- |
| 1. Notifications under the right dock | `GLCanvas3D.cpp`, `NotificationManager.cpp`, `SlicingProgressNotification.cpp` propagate and use the dock margin; narrow cards can be brought forward without overtaking an open popup. |
| 2. Ink Grouping card bottom | `BaseRenderer.cpp` measures the child content and requests another frame when its measured height changes. |
| 3. Plate name width | `OptionsGroup.hpp`, `OG_CustomCtrl.cpp` and `Tab.cpp` stack full-width Plate Settings labels and dropdowns. The existing issue record identifies `6994caf6f` and release `md3-v225`. |
| 4. Empty Slice/Print dropdown segments | The existing report reconciliation identifies an older installed build; the current implementation already contains the fix. |
| 5. Sliced status over thumbnails | `BaseRenderer.cpp` places the pill after the plate strip and omits it when the remaining canvas span is insufficient. |
| 6. Purple playback controls | The existing design article identifies this as the deliberate Preview accent. |
| 7. Objects header and frame | `GUI_ObjectList.cpp` uses `wxDV_NO_HEADER` and `wxBORDER_NONE`; `GUI_App.cpp` retains that choice. |
| 8. Dock card widths and link colour | `BaseRenderer.cpp` uses the content-region dock span; the kit supplies semantic colours. |
| 9. Canvas controls beneath the dock | Preview rail/dock geometry is retained in `GLCanvas3D.cpp` and `PreviewLayout.hpp`. |
| 10. Empty sidebar gap | The existing report reconciliation identifies an already corrected older-build behavior; sidebar integration is owned by the separate continuation lane. |
| 11. Washed-out statistics tile | `GLCanvas3D.cpp` paints the statistics glyph and label after the state wash, using opaque semantic roles. |

The full cause record remains in [Preview overlays](../features/gcode-preview/preview-overlays.md). A real post-slice layout and hardware-dependent load dialog were not exercised here. Their prior `fixed-unverified` visual status must remain until genuine runtime evidence exists.

## Shared desktop features, issue #36

The overlapping source is present: appearance editing (`Appearance/AppearanceEditorPopover.cpp`), two tone controls and emoji setting (`Preferences.cpp`, `I18N.cpp`), changelog (`ChangelogDialog.cpp`), notification centre (`NotificationCenterPanel.cpp`), tab pin/group layout (`Widgets/TabStrip.cpp`), two-key confirmation (`Widgets/SuperConfirmGate.cpp`), startup surprise (`DimSumSurprise.cpp`), display-name customization (`AppDisplayName.cpp`), exports (`Export/ExportEverything.cpp`), command palette and documentation lookup (`CommandPalette.cpp`, `CommandPaletteIndex.cpp`), and bulk ink operations (`BulkFilamentDialog.cpp`). Draft/history/sidebar integration and scheduled-setting implementation are outside this lane's write ownership. Their delivery eligibility must be established by the coordinating lanes before closing #36.

## Delivery verdict

The assigned language, kit and Preview implementation is present at the baseline plus the tone-ladder repair in this delivery. Issues #43, #45 and #51 can be considered for closure under the selected delivery criteria after the coordinator proves integration on `main` and the intended release with its downloadable required assets. This lane does not itself merge, push, publish or close an issue. Issue #36 additionally depends on the other assigned implementation lanes.
