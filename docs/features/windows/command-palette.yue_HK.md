---
translation-of: command-palette.md
source-sha256: 70bdf74401223d5f70f4c9da0e4dc9d9171cbe8a81237e269977b02dcf9263d7
review-status: agent-drafted
---

> 英文原文：[Command palette (Ctrl+Shift+F)](command-palette.md)

# 命令調色盤（Ctrl+Shift+F）

**表面**：`Ctrl+Shift+F` 喺主框架嘅任何地方 → `CommandPalette`（`src/slic3r/GUI/CommandPalette.{hpp,cpp}`）、由純數據索引喺 `src/slic3r/GUI/CommandPaletteIndex.{hpp,cpp}` 支援。

## 啟動

- `Ctrl+Shift+F` 係唯一全域快捷鍵。佢列舉喺鍵盤快捷鍵對話框（`KBShortcutsDialog`）下*全域快捷鍵*。`Ctrl+F` 唔再綁定到調色盤（同唔係綁定到框架級別嘅任何其他野）。
- 主框架安裝**一個**加速度表由 `PaletteIndex::main_frame_accelerators()` 構建：六個 `Ctrl+Numpad1..6` 標籤弦（選擇工作區標籤 N-1 當果個頁面存在）加上 `Ctrl+Shift+F`。`wxWindow::SetAcceleratorTable()` 替換而唔係合併，所以調色盤弦過去喺佢自己嘅一項目表度安裝同靜靜擦掉數字欄目項；両個家族而家住喺同一個列表，同 `tests/command_palette` 聲稱佢哋共存。
- 打開調色盤而佢已經打開係冇嘢（一個實例）。

## 大小選擇

- 一個切換按鈕喺搜尋藥丸旁邊切換兩個**有限卡**（預設，640 × ~500 dip，中心喺框架上）同**完整視窗**（覆蓋主框架嘅客戶區域）。按鈕係一個真實 MD3 IconButton 帶可訪問名稱/工具提示描述佢切換到嘅狀態（「擴展調色盤到完整視窗」/ 「收縮調色盤到卡」）。
- 選擇堅持喺 `AppConfig` 下 `palette_size`（`card` | `full`）；未知或遺漏值意思卡。`PaletteIndex::load_palette_size` / `store_palette_size` 係存儲無關所以輪換係單元測試無 AppConfig。

## 咩被索引

每一行攜帶一個 Material Symbols **圖示**、一個**標題**同一個**描述**。

| 源 | 行 | 執行 |
| --- | --- | --- |
| 快速設定 | 主題、密度、強調顏色 | 豐富內聯控制項（分段開關 / 種子樣本） |
| 工作區標籤 | 每個 `MainFrame::TabPosition` 框架實際為其構建頁面（主頁、準備、預覽、設備、多設備、項目、校準、墨水） | `select_tab()` |
| 地標 | 打開偏好、搜尋設定 | 打開偏好 / 側欄參數搜尋 |
| 偏好設定 | 每個設定偏好對話框渲染、標題為 `Preferences / <page> / <label>` | **傳送**（下面） |
| 選單欄 | 每個啟用選單項、路徑標題（`File / Import / …`） | 發佈項目嘅 `wxEVT_MENU` id |
| 文件 | 每篇文章喺 `docs/features/**`（README 索引除外）（標題只有，`Documentation / <title>`） | 喺瀏覽器度打開渲染文章（`https://github.com/Ding-Ding-Projects/BambuStudio/blob/main/<path>`，Pages 網站仲未將文章作為頁面託管） |

禁用選單項喺打開時被排除，所以調色盤永遠無法執行佢嘅選單會拒絕嘅命令。開發者頁面上嘅行只喺非公開構建中出現，符合對話框。

## 傳送

選擇偏好行唔係單純打開偏好：`GUI_App::open_preferences(key)` 打開對話框同一旦佢嘅模態環執行呼叫 `PreferencesDialog::teleport_to_setting(key)`，佢

1. 清除任何活搜尋過濾器（過濾器會隱藏行），
2. 喺導航欄同書度選擇所有者頁面，
3. 滾動行到視圖（`scroll_search_row_into_view`），
4. 焦點行嘅首個可焦點控制項（開關、組合、輸入、按鈕），
5. 帶初級角色著色行嘅標籤約 1.4 秒同恢復佢哋。

行通過 AppConfig 鍵定位：每個 `create_item_*` 構建器註冊佢返回嘅行嘅鍵（`register_option_row`），同外觀行註冊 `dark_color_mode`、`ui_density`、`ui_accent_seed`、`ui_font_family` 同 `ui_font_scale`。`build_search_index()` 摺疊果個登記到 `SearchRow::keys`。如果鍵未註冊（呢個構建中閘行），對話框退回到喺索引中記錄嘅頁面選擇。

## 搜尋

查詢欄係共用 MD3 `SearchField`：預設純文字、`.*` 正則切換同完整正則構建器霸道，調色盤遵守與每個其他搜尋欄相同嘅搜尋規則。鍵盤：輸入篩選、Up/Down 選擇、Enter 執行、Esc 關閉。行被限制到每個查詢 120 保持調色盤即時；精煉查詢揭示其餘。

## 完整性守衛

`tests/command_palette/command_palette_tests_main.cpp`（Catch2、只連結 `CommandPaletteIndex.cpp` + wx base/core）持有：

- 一份**手寫列表**嘅設定、工作區標籤、偏好頁面同必須可到達嘅文章：規則唯一測試通過一個空索引，所以列表係重點；
- 一份**源掃描守衛**：每個 AppConfig 鍵 `Preferences.cpp` 綁定行到（每個 `create_item_*` 呼叫嘅鍵參數、從 GUI 頭解析嘅命名常數、加上明確 `register_option_row("…")` 登記）必須出現喺 `preference_entries()`，反之亦然；
- 一份**目錄掃描守衛**：每個 `docs/features/**/*.md`（README 索引除外）必須出現喺 `documentation_articles()` 帶佢嘅當前 H1；
- 加速度表測試（數字欄項目同 `Ctrl+Shift+F` 一起、冇 `Ctrl+F`、唯一 ids）、大小選擇輪換同已知鍵嘅傳送目標解析（`use_inches` → 一般、`dark_color_mode` → 外觀、`backup_interval` → 其他）。

新增偏好或文章冇更新索引失敗構建嘅測試步驟而唔係寄運一個調色盤靜靜缺少佢。

## 失敗模式 / 註解

- 選單命令執行*後*調色盤關閉（發佈事件），所以模態後續（檔案對話框）從未同調色盤打架爭焦點。
- 完整視窗調色盤喺打開時從框架嘅客戶矩形大小；佢唔跟隨打開時即時框架調整。
- 文章連結指向倉庫嘅渲染 Markdown，唔係 Pages 網站，直到網站發佈文章。

## 驗證

- 捕獲（構建負載、隱藏桌面）：`docs/screenshots/md3-everything/command-palette-card--en-light-comfortable--after.png`：有限卡帶搜尋藥丸、大小切換同豐富行主題、密度同強調。

- `command_palette_tests`（Catch2）覆蓋加速度表、堅持、傳送解析同三個完整性守衛。
- 無頭驅動（Mesa llvmpipe + Lowlevel MCP）：`Ctrl+Shift+F` 打開調色盤、過濾狹窄行、Enter 喺「Go to Prepare」開關標籤、主題行嘅分段開關重新主題活 UI。布局探針嘅 `palette` 命令（`LayoutProbe.cpp`）打開佢冇弦。
