---
translation-of: bulk-actions.md
source-sha256: 3c13a7d35cecde8bcf2a627a9bf04e1b5a0ac875f1aff70297401591ee9cdc2e
review-status: agent-drafted
---

> 英文原文：[Bulk actions](bulk-actions.md)

# 批次操作

以下七個集合表面支援選取多個項目，並對選取執行操作。呢次復原唔代表已覆蓋應用程式所有其他清單或集合。

## 行為

### 選取模型

所有表面共用 `Bulk::BulkSelection<Id>` 選取模型（`src/slic3r/GUI/Bulk/BulkSelection.hpp`）。選取以穩定項目 ID 表示，永遠唔用列索引，所以重新填入、排序同改篩選後仍然保留：

| 手勢 | 效果 |
| --- | --- |
| 點擊 | 切換一個項目 |
| Shift+點擊 | 按顯示順序，選取上次點擊項目至今次項目之間嘅完整範圍，包含兩端 |
| <kbd>Ctrl</kbd>+<kbd>A</kbd> | **Select this page**：目前已繪畫嘅每一列 |
| <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>A</kbd> | **Select all N matches**：目前篩選所得全部項目，無論有冇繪畫 |
| <kbd>Ctrl</kbd>+<kbd>I</kbd> | 喺目前結果內 **Invert selection**；篩選隱藏嘅項目唔會喺背後被切換 |
| <kbd>Delete</kbd> | 開啟選取項目嘅批次刪除審閱，永遠唔直接刪除 |

全選按鈕永遠列明範圍，例如「Select this page (12)」同「Select all 340 matches」。改篩選會保留選取：已選但隱藏項目仍然選取，計數列會顯示隱藏數目。已不存在項目會從選取移除（`retain()`），避免操作過期 ID。

呢啲表面嘅搜尋列使用共用 `SearchField`，預設純文字；`.*` 開關或錨定正則建構器切換至有界限正則篩選。選取同篩選可以組合：「選晒符合呢個查詢嘅項目」就係 <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>A</kbd>。

### 操作前可審閱預覽

每個批次操作先開啟 `Bulk::BulkActionPreviewDialog`（`src/slic3r/GUI/Bulk/BulkActionPreviewDialog.{hpp,cpp}`）。對話框使用 Material Design 3 外框，列出：

- 標頭顯示操作名稱。
- **N selected / M will change / K skipped**，由計劃推導，唔係手寫，確保三個數字對得上。
- 一句清楚後果說明，即會改動嘅列將會點樣。
- 可審閱清單，包含項目、轉換後值、結果同詳細資料；有自己嘅 `SearchField` 同正則建構器，方便篩選長預覽。
- Cancel 同 Proceed。冇任何改動時，Proceed 停用，工具提示解釋原因。

唔會改動嘅每列都列明原因，例如「Skipped: file already exists」或「Skipped: preset is in use」。表面只套用計劃標示會改動嘅列，之後報告略過同失敗項目，唔會聲稱整批成功。

### 破壞性批次操作

標示破壞性嘅計劃，會令 Proceed 經錨定喺 Proceed 按鈕嘅雙鍵 `SuperConfirmGate`：先兩把鍵，再完整範圍滑動，列出受影響改動項目同確切數目。閘門 Emergency exit 同 <kbd>Esc</kbd> 將焦點還原至 Proceed。預覽對話框本身唔會刪除任何內容；閘門授權係到達表面刪除程序嘅唯一路徑。

### 按模式重新命名

`Bulk::BulkRenameDialog`（`src/slic3r/GUI/Bulk/BulkRenameDialog.{hpp,cpp}`）透過 `BulkRenamePattern.hpp` 引擎重新命名選取：

| 欄位 | 意思 |
| --- | --- |
| Pattern | `{name}` 保留目前名稱，`{n}` 由開始索引編號，`{i}` 由 0 開始，`{stem}` 同 `{ext}` 分開檔名；`{{` 同 `}}` 代表字面大括號 |
| Find / Replace | 取代每次出現。搜尋欄係 `SearchField`；`.*` 開關或正則建構器啟用 ECMAScript 正則，支援 `$1`..`$9` 反向參照同大小寫敏感 |
| Start `{n}` at / Zero-pad | 流水索引起點同寬度 |

之前 → 之後預覽每次按鍵都重新計劃。名稱冇改、結果空白或正則格式錯誤時會略過該列。**碰撞**，即兩列產生同名，或結果等於集合已有名稱，會停用 Rename，並喺兩列都標明，確保重複任何一半都唔會套用。Pattern、find 同 replace 限 512 位元組，名稱限 4096 位元組。正則取代使用原有受限工作程序同總截止時間，拒絕不完整比對集合。正則錯誤報告為無效列，唔會拋出例外。

### 長時間操作

`BulkActionPreviewDialog::RunWithProgress` 喺套件 `ProgressDialog` 背後執行套用迴圈，顯示經過同剩餘時間、目前項目名稱（例如「3 of 40: cube.stl」），Cancel 會喺目前項目完成後停止。呼叫端收到已完成步數、係咪取消同失敗步數，並喺非阻塞通知報告三者。

### 復原

表面原本有歷史紀錄時，批次操作使用同一路徑：物件清單批次重新命名同批次刪除只取一個 undo／redo 快照；通知中心刪除前先匯出保留副本；預設同設定檔快照由表面原有本機 Git 歷史記錄。無法復原嘅表面，會喺後果列講清楚。

## 各表面接線

以下每個表面都用共用 `BulkSelection`、可審閱預覽，破壞性計劃亦用雙鍵閘門。「唔提供」屬刻意決定並附原因，避免缺少操作似係遺漏。

| 表面 | 檔案 | 選取 | 已接線批次操作 | 唔提供及原因 |
| --- | --- | --- | --- | --- |
| 物件清單 | `GUI_ObjectList.cpp`、`GUI_Factories.cpp`（內容選單 **Bulk** 群組） | 原生多選；**Select all objects**（Ctrl+A）、**Select all matches**（Ctrl+Shift+A；清單冇篩選，所以等於全選並會講明）、**Invert selection**（Ctrl+I） | **Bulk rename…**（`BulkRenameDialog`、一個復原快照）、**Bulk delete…**（預覽、破壞性閘門、一個復原快照）、**Bulk export…**（透過 `Plater::export_object_stl` 每物件一份 STL，存入所選資料夾；已有檔案略過，進度可取消） | 移動／複製／標籤：物件喺呢個表面冇呢啲操作 |
| 側欄耗材（ink）列 | `Plater.cpp`、`BulkFilamentDialog.cpp` | 每槽核取方塊、全選、**Invert selection** | 暫存批次（套用預設、套用顏色、刪槽、新增耗材）顯示為一份計劃，逐槽有結果；有刪槽嘅批次屬破壞性，要經閘門；最後槽會具名略過 | 匯出：耗材槽屬專案狀態，隨專案匯出 |
| 使用者預設 | `UserPresetsDialog.cpp`（亦可由預設組合方塊編輯選單 **Batch Preset Management…**，`PresetComboBoxes.cpp` 到達） | 核取列；**Select visible**（Ctrl+A）、**Select all matches**（Ctrl+Shift+A）、**Invert**（Ctrl+I）、Delete | **Delete**（預覽、破壞性閘門）、**Export selected…**（每預設一份 `.json` 存入所選資料夾，已有檔案略過，進度可取消）、**Rename selected…**（`BulkRenameDialog`，與任何預設名稱碰撞都拒絕，進度可取消） | 移動／複製：每個預設只屬一個集合 |
| 專案版本歷史 | `ProjectHistoryDialog.cpp`、`ProjectHistoryManager.cpp` | 原生多選；**Select visible (N)**（Ctrl+A）、**Select all N loaded/versions**（Ctrl+Shift+A，文字列明有冇舊版本仍未載入）、**Invert**（Ctrl+I） | **Export selected…**（每版本一份 `.3mf` 存入所選資料夾，進度可取消）、**Label selected…**（`label_version`，每版本一個輕量標籤；重複標籤報告為失敗操作） | 刪除：歷史設計只追加。還原：只限單一版本，按鈕工具提示解釋原因 |
| 通知中心 | `NotificationCenterPanel.cpp`、`NotificationHistory.cpp` | 核取列配 shift 範圍；**Select this page (N)**（Ctrl+A）、**Select all N matches**（Ctrl+Shift+A）、**Invert selection**（Ctrl+I）、**Clear selection**、Delete | **Dismiss selected**、**Export…**（遵從目前篩選，四種格式）、**Delete selected…**（預覽、破壞性閘門；先提供匯出保留副本） | 重新命名／移動：項目係不可變紀錄 |
| 多機管理器 | `MultiMachineManagerPage.cpp` | 卡片核取方塊，shift 範圍跨目前已繪畫頁面；**Select this page**、**Select all matches**（所有頁面符合搜尋嘅裝置）、**Invert selection**、**Clear**（Ctrl+A／Ctrl+Shift+A／Ctrl+I） | **Export selected…**（名稱、id、型號、狀態、工作、進度匯出 JSON 或 CSV，先預覽） | 刪除／重新命名：裝置綁定帳戶，名稱喺印表機設定，頁面唔擁有佢哋。批次傳送列印已喺多機工作頁面提供 |
| 設定檔 | `ConfigProfilesDialog.cpp` | 原生多選；**Select visible**（Ctrl+A）、**Select all**（Ctrl+Shift+A）、**Invert selection**（Ctrl+I） | **Snapshot selected**（每設定檔一個完整快照，經預覽同可取消進度對話框）、**Export list…**（名稱、資料資料夾、最後快照匯出 JSON 或 CSV） | 刪除：呢個表面冇單項或批次刪除；要喺應用程式外刪資料夾先移除設定檔。Launch 保持單列，因為會啟動另一實例 |

各表面工具提示顯示嘅捷徑，正正係該表面 `wxEVT_CHAR_HOOK` 處理器綁定嘅捷徑。文字控件有焦點時，Ctrl+A 保留作文字全選，Delete 保留作編輯，所以清單本身有焦點先會觸發清單捷徑。

## 設定

冇需要設定嘅項目。捷徑固定為以上集合，並喺每個表面工具提示同內容選單顯示，確保顯示捷徑就係該表面實際觸發嘅捷徑。

## 失敗模式

- 空選取：批次按鈕停用，工具提示列明需要選取。
- 冇改動：Proceed 停用，每列顯示略過原因。
- 長執行期間某步失敗：繼續執行，計入失敗並喺結束通知具名報告；取消會喺目前項目之後停止，並報告完成程度。
- 重新命名碰撞或無效正則：Rename 保持停用，錯誤列喺計數下方；輸入唔會丟棄。
- 破壞性閘門任何階段取消，都唔做操作，焦點返回來源按鈕。

## 安全考慮

篩選正則評估使用有界限 `SearchField::MatchPass`（模式同主體尺寸上限、逾時、工作程序隔離）。重新命名引擎用 `std::regex`，模式同主體有硬性尺寸上限；只對所選名稱執行模式，批次對話框輸入唔會傳送或持久化。匯出只寫入使用者喺原生對話框選擇嘅路徑，永遠唔會默默覆寫已有檔案；已有檔案會喺預覽列為略過。

## 交付驗證狀態

呢次復原由已保留九月實作移植至目前交付原始碼。10 月 5 日極速交付流程冇執行測試、lint、型別檢查、執行互動或畫面擷取。已保留草稿嘅歷史測試聲明，唔係呢個候選嘅證據。建置同打包結果由版本交付工作另行報告。

專案歷史保留多儲存庫篩選、比較同完整清單匯出。批次選取鍵包含儲存庫身分同裝置；匯出同標籤會將每個版本交畀所屬歷史管理器。只有事故紀錄嘅項目冇快照儲存，會明確略過。已儲存搜尋歷史仍係獨立表面，保留原有個別操作；呢次復原唔會為已儲存搜尋、印表機事故列或無關集合新增批次操作。

## 建議文章

- [通知中心](notification-center.md)：第一個使用共用選取模型同經閘門批次刪除嘅表面。
- [專案版本歷史](project-version-history.md)：批次匯出版本，同唔提供批次刪除嘅原因。
- [設定檔同完整資料備份](config-profiles-backup.md)：批次快照同清單匯出。
- 破壞性操作超級確認：每個破壞性批次操作都要經雙鍵閘門。
