---
translation-of: preferences-history.md
source-sha256: 23b1327b79e74cc26a9868f99c8bbe2e4cd70b47e2ee835865cc26655634ed15
review-status: agent-drafted
---

> 英文原文：[Preferences auto-history](preferences-history.md)

# 偏好設定自動歷史記錄

**表面：** 自動（每個設定儲存）；瀏覽器喺 `檔案 ▸ 組態設定檔和備份… ▸ 偏好設定歷史…`（`src/slic3r/GUI/PreferencesHistory.{hpp,cpp}`、UI 喺 `ConfigProfilesDialog`）。

## 行為

- 每個成功嘅 `AppConfig::save()` 觸發儲存觀察者（`AppConfig::set_save_observer`，喺 `GUI_App::post_init` 安裝），該觀察者排定 `BambuStudio.conf` 嘅 **防抖動（2 秒）快照** 進入隔離本地 Git 存儲庫：與組態設定檔相同嘅 `ProjectHistoryManager` 引擎和存儲根（`<data-dir-parent>/BambuStudio-profiles/`、身份 `preferences.history`）。相同快照喺引擎內重複刪除，因此一次儲存突發最多成本一次提交。
- **偏好設定歷史…** 列出快照（時間戳記、訊息、提交 id）。恢復寫入 `BambuStudio.conf.restored-<commit>` **旁邊** 實時檔案：實時組態永遠唔會喺執行應用程式下被替換；狀態行告訴用戶喺應用程式關閉時交換檔案。

## 故障模式

- 存儲庫初始化故障 → 觀察者降級為無操作，瀏覽器報告引擎錯誤；應用程式本身不受影響。
- 歷史記錄保持本地：永遠唔同步、永遠唔推送、永遠唔喺匯出嘅資料目錄 zip 內（存儲根住喺資料目錄旁邊）。

## 驗證

- 內置於 `libslic3r_gui`；無頭煙霧：改變偏好設定會喺防抖動後產生瀏覽器中可見嘅 `Preferences change` 提交；恢復寫入兄弟副本並保留實時 conf 未觸及。
