# Continuity delivery

This continuation implements the two source gaps identified in the preserved #56 notes. It does not claim runtime or hardware acceptance. The owner selected implementation, main integration and release delivery for this speed pass; tests, lint, reviews and captures are intentionally not run.

## Changes

- `MainFrame.cpp`: H2S PPA-CF/PPS-CF first-use guidance now uses the existing non-blocking brittle-filament notification. Selecting its Wiki link is optional and does not prevent the queued slice action. The plate's existing checked material helper replaces unchecked filament indexing.
- `MD3ScrolledWindow.cpp`: an embedded zero-rate settings page forwards its original wheel event to its designated reveal owner. Axis, delta, modifiers and coordinates survive; standalone scrollers keep native handling.
- Camera: the preserved continuation lists build and interaction evidence still needed, not an unfinished implementation requirement. Continuous playback, explicit pause, retry classification and per-printer pan/zoom already live in `MediaPlayCtrl`, `CameraPlaybackPolicy`, `CameraViewGeometry`, `wxMediaCtrl3`, `CameraPopup` and `CameraHUD`. No duplicate camera change was made.

## Overlapping requests

| Request | Existing source and delivery mapping | Remaining boundary |
| --- | --- | --- |
| #41 upstream/native controls | Upstream 2.8.4.57 integration, `MD3Menu`, nozzle preferences, `MainFrame` slice/print/send action routing, `StatusPanel`, LAN farm paths, `ModelCreator/ModelCreatorDialog` | Build, package and release verdict belong to the integrated candidate, not this narrow source commit. No physical print is submitted by this task. |
| #41 portable workspace and history | `WorkspacePanel`, `WorkspacePlanner`, workspace bundles, `ProjectHistoryDialog` and local history manager; documentation under `docs/features/workspace/` | The source bridge embeds history in each member 3MF, copies that history-bearing member into the workspace ZIP and imports it when reopened. Runtime proof is intentionally not produced in speed mode. |
| #36 Prepare scroll ownership | `Plater` sidebar virtual-height layout, `ParamsPanel` reveal ownership, `MD3ScrolledWindow` and this wheel forwarding | Native wheel/focus/resize evidence is not produced in speed mode. |
| #36 native context menus and desktop feature set | `MD3Menu`, Preferences appearance/funny/emoji/tab surfaces, changelog and notification center, confirmation, export, offline documentation and palette modules | Exact named coverage and preserved missing modules are recorded below. Build/release delivery remains separate from source presence. |

Historical continuation files retain their original source-specific counts and states. Their earlier tests are not new evidence for this candidate. Native lifetime and readable-date continuations currently identify missing runtime/compile evidence rather than another concrete source gap assigned here. Broader product requests in #41/#36 are not silently declared fulfilled from this bounded change.

## Cantonese handoff

今次修兩個保留交接中列明嘅原始碼缺口：H2S 材料教學唔再彈模態視窗阻住切片，內層設定頁嘅滾輪事件交返外層側欄。鏡頭功能已有程式，交接列明嘅係建置同操作證據，唔會再寫一套重複功能。今次快速交付刻意唔跑測試、檢查或擷圖；main、安裝包同發佈結果由整合交付記錄。

## Named #36 delivery inventory at 9b263774e

| Named feature | Exact source delivery | State |
| --- | --- | --- |
| Per-element appearance | `Appearance/AppearanceEditorPopover.cpp`, `Appearance/ElementStyle.cpp` | Present |
| English/Cantonese funny sliders | `PreferencesDialog::create_item_funny_level_slider`, `LanguageMode.cpp` | Present |
| Dialog emoji toggle | `Preferences.cpp` dialog-emojis checkbox, `LanguageMode.cpp` | Present |
| Changelog viewer | `ChangelogDialog.cpp`, Help menu in `MainFrame.cpp` | Present |
| Notification centre | `NotificationCenterPanel.cpp`, `NotificationHistory.cpp` | Present |
| Pin/group settings tabs | `Preferences.cpp` stable section IDs and `Widgets/TabStrip.cpp` persisted pins/groups | Present |
| Two-key super confirmation | `Widgets/SuperConfirmGate.cpp`, delete actions in `Plater.cpp`, `Tab.cpp`, notification centre | Present |
| Startup dim sum | `DimSumSurprise.cpp` remains implemented; later #56 quiet-default scope removed automatic startup invocation | Intentionally superseded by quiet workflow |
| Rename display name | `Preferences.cpp` name/reset actions, `GUI_App::set_app_display_name`, `MainFrame::on_app_display_name_changed` | Present |
| Scheduled settings | `Schedule/ScheduledSettings.cpp`, model and panel absent on current main | Preserved source at `29209a60b`, pending scoped integration |
| Export formats | `Export/ExportEverything.cpp` and its native dialog | Present |
| Offline documentation browser | `DocsBrowserDialog.cpp`, `Markdown.cpp`, `BundleDocs.cmake` absent on current main | Preserved source at `1498551fe`, pending scoped integration |
| Command palette | `CommandPalette.cpp`, `CommandPaletteIndex.cpp`, main-frame accelerator | Present; native offline article action belongs to pending browser integration |
| Bulk actions | `BulkFilamentDialog.cpp`, `NotificationCenterPanel.cpp` subsets present | All-list implementation at `dcc5c0926` remains pending scoped integration |

The three preserved tips above are evidence of recoverable implementation, not evidence that their code is delivered on main. They overlap current source and must be reconciled with the current candidate. The scheduled-settings tip also carries Home Assistant sync, which is not automatically adopted into this task.

## Portable history source bridge for #41

`MainFrame::save_active_workspace_member` calls `Plater::export_workspace_member_with_history`. That method flushes pending local events, writes a geometry snapshot, commits it to the stable tab-owned history identity, publishes the portable history into that 3MF and copies the complete file to an immutable candidate. `WorkspacePanel::save_member` stages that file; `Workspace::save_bundle` copies it as `Members/<id>/project.3mf`, including `Metadata/bambu_project_history.json` and `.pack`. Member validation uses the portable-history inspector. Reopening a workspace member through `MainFrame::open_workspace_member` reaches the project loader, which calls `ProjectHistoryManager::import_portable_history`. Normal saved 3MF publication likewise invokes `publish_portable_history`. The embedded pack carries local history; a separate external history-store folder is not required to transport it. No source bridge repair is needed for this request.
