---
translation-of: atlas-scroll-owner-adoption.md
source-sha256: b7ab7b3dd5c7a88a78282171920197295c526fe4da298e17f6ad3759b7b27a0a
review-status: agent-drafted
---

> 英文原文：[Atlas scrollbar owners in workflow and bulk callers](atlas-scroll-owner-adoption.md)

# 工作流程同批次操作呼叫端嘅 Atlas 捲軸擁有者

整合捲軸掃描器喺六個呼叫位置發現直接使用 wxWidgets 捲軸擁有者。呢項修正改用現有套件擁有者，唔改佢哋嘅實作，亦唔會削弱掃描器。

| 呼叫端 | 取代嘅原有擁有者 | 保留嘅呼叫端合約 |
| --- | --- | --- |
| `Schedule/ScheduledSettingsPanel.cpp/.hpp` | 排程編輯器捲動內容同繼承式設定面板改用 `MD3ScrolledWindow` | 原有父層、樣式旗標、12 DIP 捲動步長、規則模型、搜尋、選取同排程回呼 |
| `WorkflowPrintPanel.cpp/.hpp` | 繼承式列印檢閱面板改用 `MD3ScrolledWindow` | 原有父層、垂直捲動同鍵盤遍歷旗標、16 DIP 步長、摘要更新、尺寸同經確認嘅操作路徑 |
| `WorkspacePanel.cpp` | 分區頁面工廠建立 `MD3ScrolledWindow` | 原有分區身分、父層、旗標、16 DIP 步長、擴展佈局、更新／重新排版同工作區資料回呼 |
| `Bulk/BulkActionPreviewDialog.cpp` | 預覽表格建立 `MD3DataViewListCtrl` | 原有要求高度 220 DIP、旗標、欄位、列模型、選取同確認回呼 |
| `Bulk/BulkRenameDialog.cpp` | 重新命名預覽表格建立 `MD3DataViewListCtrl` | 原有要求高度 200 DIP、旗標、欄位、重新命名計劃列、選取同套用回呼 |

原始碼差異涉及七個檔案：兩個繼承擁有者標頭同五個實作檔案。只改擁有者名稱同必要套件標頭。Workspace 嘅 `wxScrolledWindow*` 儲存，同兩個批次對話框嘅 `wxDataViewListCtrl*` 成員保持不變。套件擁有者公開繼承呢啲基底類別，接受呢度使用嘅相同建構參數。

`MD3ScrolledWindow` 保留 wx 捲動輔助器，將捲軸狀態交畀 `MD3ScrollBars`，並繼承原有鍵盤、滾輪同子控件焦點顯示行為。`MD3DataViewListCtrl` 保留資料檢視模型同表格行為，將捲軸狀態交畀同一套件擁有者，並保留原有方向鍵處理。呢項修正冇新增捲動引擎、取代模型、設定顯示擁有者、改回呼綁定或重新設定呼叫端尺寸。套件本身實作保持不變。

## 針對性驗證

已報告嘅整合基準 `a271602901c1b079a342dc9ea89edca57c5345c6` 未通過直接使用捲動視窗同資料檢視嘅掃描器檢查。修正後，指定嘅完整選擇通過：

```powershell
node --test ui-md3/tests/native-controls.test.mjs ui-md3/tests/scrollbars.test.mjs ui-md3/tests/native-feature-delivery.test.mjs
```

結果：29 項檢查通過，包括 10 項原生控件檢查、10 項捲軸檢查同九項功能交付檢查。冇改掃描器檔案或排除項目。

```powershell
node --test tests/native_shared_controls/atlas_scroll_owner_adoption.test.mjs tests/workspace/workspace_panel_atlas.test.mjs tests/native_preferences_atlas.test.mjs
```

結果：24 項檢查通過，包括九項擁有者採用檢查、八項 Workspace 檢查同七項偏好設定／排程檢查。新檢查只反向還原已批准嘅擁有者／標頭替換，然後將每個改動原始碼檔案同確切基準比較。任何建構參數、回呼、模型操作、尺寸、樣式旗標或捲動步長陳述改動，都會喺比較中保留。負面檢查會拒絕原始擁有者、已改控件識別碼同已改排程回呼。基底類別相容性同未改指標宣告另外檢查。現有 Workspace 檢查保留 30 個行為方法、事件綁定、頁面身分同內容佈局生命週期。

以上係原始碼檢查。冇執行完整應用程式建置、啟動、安裝程式、硬件操作或實際畫面擷取。原生捲軸繪畫、滾輪／翻頁／鍵盤移動、子控件焦點顯示、選取同實際顯示縮放下嘅佈局仍未驗證。已納入版本控制嘅 Atlas 合約提供設計指引，唔係執行期間證據。

## 還原同範圍

呢個獨立擁有者採用提交可以還原，唔需要改套件實作或資料模型。還原會恢復原始捲軸擁有者同掃描器失敗。其他控件家族、呼叫端重設計工作，同全應用程式執行期間一致性矩陣，仍然唔屬於呢項修正。
