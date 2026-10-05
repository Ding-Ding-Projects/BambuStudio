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
| #41 portable workspace and history | `WorkspacePanel`, `WorkspacePlanner`, workspace bundles, `ProjectHistoryDialog` and local history manager; documentation under `docs/features/workspace/` | Portable workspace packaging and history are separate containers and must not be represented as a proven single-file round-trip by this change. The history/draft implementation lane owns current integration changes. |
| #36 Prepare scroll ownership | `Plater` sidebar virtual-height layout, `ParamsPanel` reveal ownership, `MD3ScrolledWindow` and this wheel forwarding | Native wheel/focus/resize evidence is not produced in speed mode. |
| #36 native context menus and desktop feature set | `MD3Menu`, Preferences appearance/funny/emoji/tab surfaces, changelog and notification center, confirmation, export, offline documentation and palette modules | This lane does not implement or assert complete acceptance of every historical feature row. Remaining release acceptance is tracked by the main delivery task. |

Historical continuation files retain their original source-specific counts and states. Their earlier tests are not new evidence for this candidate. Native lifetime and readable-date continuations currently identify missing runtime/compile evidence rather than another concrete source gap assigned here. Broader product requests in #41/#36 are not silently declared fulfilled from this bounded change.

## Cantonese handoff

今次修兩個保留交接中列明嘅原始碼缺口：H2S 材料教學唔再彈模態視窗阻住切片，內層設定頁嘅滾輪事件交返外層側欄。鏡頭功能已有程式，交接列明嘅係建置同操作證據，唔會再寫一套重複功能。今次快速交付刻意唔跑測試、檢查或擷圖；main、安裝包同發佈結果由整合交付記錄。
