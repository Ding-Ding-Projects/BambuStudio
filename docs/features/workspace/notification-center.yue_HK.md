---
translation-of: notification-center.md
source-sha256: 79fccaadccad53d4753df9d934771375ee554dc89467b66793db7b0ee0c331b3
review-status: agent-drafted
---

> 英文原文：[Notification centre](notification-center.md)

# 通知中心

通知中心係每條桌面應用顯示過嘅 toast 嘅可檢視歷史。[非阻塞通知](non-blocking-notifications.md)入面描述嘅 ImGui 角落通知會自己消失，呢到係被解除嘅通知仲可以被閱讀、搜尋、匯出或刪除嘅地方。佢從頂部欄上面嘅鈴聲打開（`BBLTopbar`，外觀按鈕左邊）做非模態 Material Design 3 彈出框錨定喺鈴聲下面。

源碼：`src/slic3r/GUI/NotificationHistory.{hpp,cpp}`（模型），`src/slic3r/GUI/NotificationCenterPanel.{hpp,cpp}`（彈出框），`src/slic3r/GUI/NotificationManager.{hpp,cpp}`（錄製掛鈎），`src/slic3r/GUI/BBLTopbar.cpp`（鈴聲 + 徽章）。

## 行為

### 錄製

- `NotificationManager` 擁有一個 `NotificationHistory`。每條變成新 toast 嘅通知（`push_notification_data`，附加到 `m_pop_notifications` 嘅分支）附加一個記錄：id、UTC 時間戳（毫秒）、等級同佢嘅穩定名稱（`regular`、`important`、`warning`、`serious_warning`、`error` 等等）、`NotificationType` 枚舉名稱、標題（文字嘅第一行）、完整文字（`text1` 加 `text2`）、已解除旗標、已看旗標同採取嘅動作。
- 進度條 toast（`ProgressBarNotificationLevel`）**唔係**被錄製：佢哋喺每次跳動時重新推送同會淹冇清單。已經可見 toast 嘅更新（`activate_existing`）都唔會建立第二行。
- 當 toast 因任何原因離開螢幕時被標記為已解除：用戶閂咗佢、佢褪咗出去或經理刪除咗佢程式化（`update_notifications`、`close_and_delete_self`、`remove_notification_of_type` 同 `AssemblyInfo` / `BBLObjectInfo` 嘅原地更換）。
- 點擊 toast 嘅超文字記錄超文字標籤做採取嘅動作（`on_text_click` / `on_second_text_click`）。
- 歷史係只追加同被限制到 500 個條目（`NotificationHistory::DEFAULT_MAX_ENTRIES`）；最舊嘅記錄掉咗。Id 係單調嘅同永唔會被重用，即使喺修剪或重新載入後。

### 鈴聲同徽章

- 當冇未讀嗰陣時鈴聲顯示 `notifications`，否則 `notifications_active` 加一個錯誤填充徽章帶著計數（限制喺 `99+`）。「未讀」表示「自從中心上次被打開後到達」；打開中心標記所有事項被看同清除徽章。
- 懸停同外觀按鈕一樣畫相同圓形鬼影圓盤。工具提示/可訪問名稱讀做 `Notifications` 或 `Notifications (N unread)`。
- 鈴聲喺 Material Symbols 面被守護完全像外觀按鈕：唔得字型兩個都唔會被加。

### 彈出框

- 一個 `MD3Dialog`（可調整大小變體）顯示非模態、定位喺鈴聲下面同限制到顯示嘅客戶區域。標頭關閉按鈕同 <kbd>Escape</kbd> 隱藏佢；鈴聲切換佢。過濾、頁面同選擇倖存隱藏/顯示。
- **搜尋**：共享 `SearchField` 帶著正則表達式建立者。純文字係預設；`.*` 切換或 `tune` 彈出框啟用正則表達式、大小寫、完整詞同多行。匹配通過每次刷新一個 `SearchField::MatchPass` 超過標題、文字、類型名稱、等級名稱同動作。查詢同等級芯片同已解除切換組合，永唔會覆蓋佢哋。
- **等級芯片**：所有等級 · 資訊（提示、正規、打印資訊） · 重要 · 警告（警告、嚴重警告） · 錯誤。**顯示已解除**切換是否列表已關閉 toast。
- **清單**：最新優先，每個記錄一行帶著等級、時間（本地）、標題、詳細資料（剩餘行）、狀態（活躍/已解除）同採取嘅動作。佢係 `wxDataViewListCtrl` 喺多選模式：點擊、<kbd>Shift</kbd>+點擊範圍、<kbd>Ctrl</kbd>+點擊、方向鍵、<kbd>Ctrl</kbd>+<kbd>A</kbd>（選擇**呢頁**）、<kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>A</kbd>（選擇**所有匹配**）。行被渲染 100 次一次；**顯示 N 更多**擴展頁面。每行係可聚焦同控件帶著可訪問名稱 `Notification history`；MSW 嘅數據視圖暴露每行嘅儲存格文字到螢幕閱讀器。
- **選擇行**：`Select this page (N)` 同 `Select all N matches` 係兩個唔同動作帶著清單中嘅計數，所以用戶永遠知道是否被選擇呢個渲染切片或每個匹配。`Invert selection` 喺現時匹配內翻轉；`Clear selection` 清空佢。選擇被保持做 id 嘅集合，所以佢倖存自動刷新、分頁同過濾更改（掉出過濾嘅 id 保持被選擇但唔被計數或作用）。
- **批量解除**：關閉任何仲喺螢幕上面嘅選定 toast 同標記記錄已解除。
- **批量匯出**：JSON、CSV、Markdown 或純文字，通過儲存對話嘅檔案類型過濾選擇。匯出選擇或每個匹配當冇都被選擇時，永遠尊重活躍過濾。每個匯出聲明佢嘅範圍喺標頭：`Exported N of M recorded notifications; query: …; levels: …; status: …; time span <oldest> to <newest>; exported at …; encoding UTF-8; schema v1`。時間戳係 ISO-8601 UTC 帶著毫秒。
- **批量刪除**：打開內聯 ErrorContainer 卡叫做確切計數（「Permanently delete N notification entries …」）後面 `SlideToConfirm` 門（危險樣式、鍵盤可操作：箭頭行走旋鈕、<kbd>End</kbd> 完成、<kbd>Home</kbd> 重設）帶著取消按鈕。刪除都關閉任何匹配直播 toast。門喺選擇清空時自動被提取當佢打開時。焦點返回刪除按鈕後。`// TODO(SuperConfirmGate)` 喺面板記號一旦呢個車道著陸交換到兩鍵超級確認門。
- **自動刷新**：當顯示時，1 秒計時器比較 `NotificationHistory::revision()` 同當佢改變時重新填充；選擇同頁面限制被保持。
- **空狀態**：`No notifications match. New toasts will appear here as they are shown.` 當過濾產生冇都替換清單。
- 禁用批量按鈕帶著工具提示命名未滿足條件（`Select at least one notification first`、`Nothing matches the current filter`）。

## 設定

- 持久性檔案：`<data_dir>/notification_history.json`（旁邊 `BambuStudio.conf`）。寫原子化（`.tmp` 然後重名）喺每個更改；當 `NotificationManager` 被構造時載入一次。
- 被限制：500 個條目、固定喺代碼。頁面大小：100 行、固定喺代碼。
- 冇用戶設定禁用錄製；中心係通知系統嘅部分，唔係選擇進入功能。

## 失敗模式

- 缺失歷史檔案：正常首次執行，開始空。
- 格式不正確或更新版本模式檔案：未被載入、警告被記錄、應用開始帶著空歷史同下一個更改覆蓋檔案。
- 儲存失敗（唯讀資料夾、磁碟滿）：喺警告等級記錄；記憶體中歷史保持工作同下一個更改重試。
- 匯出寫失敗：錯誤 toast 命名路徑同問用戶檢查資料夾；冇都被部分聲稱。
- 正則表達式超過有限正則表達式預算：`SearchField::MatchPass` 把行當做匹配（fail-open，如同到處建立者被使用）所以壞模式永唔會靜靜隱藏條目。
- 刪除仲喺螢幕上面 toast 嘅記錄關閉該 toast 都，所以清單同角落永唔會唔同意。

## 安全考慮

- 歷史係純 JSON 喺本地磁碟；佢包含任何應用放喺 toast 嘅文字（檔案路徑、打印機名稱、錯誤字串）。佢永唔同步、上傳或自動包含喺蟲報告。
- 匯出被寫只到用戶選定嘅路徑。CSV 欄被引用根據 RFC 4180 同 Markdown 管道被逃逸，所以 toast 文字唔可以打破表格或注入公式樣子儲存格。
- 正則表達式評估通過共享有限引擎（截止同隔離工作員），所以病態模式唔可以懸起 UI。
- Toast 文字喺清單中被渲染做純文字；冇都被解釋做標記。

## 確認

- `tests/notification_center/`（Catch2 目標 `notification_center_tests`）覆蓋追加次序、解除冪等式、動作、已看/未讀、500 限制、擦除、JSON 持久性往返旅行同拒絕格式不正確輸入、過濾組合（查詢 × 等級 × 狀態、自訂配對程式、最新優先）、選擇所有頁面對比選擇所有匹配、逆向選擇、移位範圍同 CSV / JSON / Markdown / 純文字匯出形狀包括範圍標頭。建立同執行用手喺 2026-09-08：**154 斷言喺 11 測試情況，全部通過**。
- `cl /Zs` 語法檢查通過針對 `NotificationHistory.cpp`、`NotificationCenterPanel.cpp`、`NotificationManager.cpp` 同 `BBLTopbar.cpp`  針對 `libslic3r_gui` 包括集。
- 尚未完成喺呢個車道：完整應用建立同無頭鈴聲、彈出框、空狀態同刪除門擷取。呢啲屬於整合通過。

## 建議文章

- [非阻塞通知](non-blocking-notifications.md)：中心記錄嘅 toast。
- [偏好設定自動歷史](preferences-history.md)：其他只追加本地記錄帶著瀏覽器同過濾。
- [設定檔案 & 完整數據備份](config-profiles-backup.md)：其他破壞性流被 `SlideToConfirm` 守護。
