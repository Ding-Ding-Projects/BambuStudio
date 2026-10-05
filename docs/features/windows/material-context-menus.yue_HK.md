---
translation-of: material-context-menus.md
source-sha256: 46505612ee158cd37419eac0486b6a984372bdf527febee5044a8b81c283da31
review-status: agent-drafted
---

> 英文原文：[Material context menus](material-context-menus.md)

# 物料脈絡選單

桌面應用每個脈絡選單由Material Design 3 選單視窗 (`src/slic3r/GUI/Widgets/MD3Menu.{hpp,cpp}`) 渲染而唔係本地 Windows 彈出。`wxMenu` 保持數據模型：ids、標籤、加速器、檢查同埋無線電狀態、`wxEVT_UPDATE_UI` 啟用規則同埋子選單全部保持工作未變、同埋每個現存 `Bind(wxEVT_MENU, …)` 處理器仍然激發。

## 行為

- **表面。** 表面容器填充、1 px 大綱變異框架、12 dp 角、24 dp 前導槽位作為一個圖示、檢查記號或無線電字形、主體小標籤、字幕大小快捷鍵右對齏、子選單列嘅 V 形、懸停上 8% 狀態層同埋次要容器填充鍵盤選擇。禁用列保持佢們嘅標籤以減少強調。分隔符係 1 px 線帶 8 dp 填充同埋當篩選會留下佢們領先、尾隨或雙時摺疊。
- **搜尋欄位。** 每個選單，包括只有一個項目嘅選單同每一層子選單，都有共用 `SearchField` 搜尋欄同正則構建器。搜尋只篩選本地項目，唔會改變命令。每層保留自己嘅查詢；冇結果會顯示「No matches.」對應嘅本地化文字，唔再按項目數量隱藏搜尋功能。
- **快捷鍵。** 每列顯示喺佢嘅 `wxMenuItem` 上登記嘅快捷鍵（從加速器解析、落回到標籤嘅 `\t` 尾碼）所以選單文件更快路線到每個命令佢列表。螢幕讀者通過 `GetKeyboardShortcut` 接收佢、唔係額外標籤文字。
- **子選單。** 打開到父列右邊、頂端對齏、喺顯示邊緣翻轉到左邊；懸停打開後 200 ms（減少運動下立刻）、Right 或 Enter 打開、Left 關閉一層；Escape 先清除非空查詢，再按一次先關閉一層。父拒絕當子開放時關閉。
- **邊界。** 表面絕唔覆蓋佢嘅錨：放置嘗試下、上、右同埋左、然後到顯示、同埋選單比自由高度更高捲動喺佢嘅卡內。
- **鍵盤同埋焦點。** 上同埋下同埋首同埋尾同埋頁向上同埋頁向下通過可動作列移動、Enter 或 Space 激活、Escape 先清除非空查詢再關閉，外部點擊直接關閉、Tab 同埋 Shift+Tab 通過搜尋輸入、佢嘅可見正則、構建器同埋清晰控件迴圈、然後選單列表；焦點回到選單打開時有佢嘅任何事。助記符（`&E`）只當搜尋欄位空時激活。
- **激活順序。** 項目嘅 `wxEVT_MENU` 處理器執行同步喺阻止 `MD3::PopupMenu` 呼叫返回之前、匹配本地 Windows 順序。檢查項翻轉佢們嘅 `wxMenuItem` 狀態事件被發送之前。
- **語言模式。** 喺雙語模式一列顯示 `label · secondary` 當佢適應同埋移動次要文字到列工具提示否則。搜尋佔位符同埋空狀態係本地化。

## 設定

冇設定；視窗取代本地彈出處處。常數喺 `MD3Menu.hpp`：`kSubmenuHoverDelayMs`（200）、`kDrawShadow`（假：框架專用提昇直到一個分層視窗陰影著地）。

代碼入點：`MD3::PopupMenu(owner, menu, screen_pos)`（阻止、發送事件）、`MD3::PopupMenuSelection(owner, menu, screen_pos)`（阻止、返回 id、發送無）、`MD3::PopupMenuBelow(anchor, menu, show_search)`（錨喺按鈕下方；舊有可選標誌保留相容性，但搜尋欄一直顯示）。`Plater::PopupMenu` 通過第一個路由、所以對象列表、預設組合框、3D 場景同埋墨水列晒共用一個表面。托盤圖示嘅 `wxTaskBarIcon::CreatePopupMenu` 係唯一深思熟慮例外：殼層擁有那個彈出。

## 失敗模式

- 一個處理器銷毀擁有者視窗而選單打開：彈出綁定擁有者嘅 `wxEVT_DESTROY` 同埋關閉佢自己；嵌套事件迴圈恰好一次退出。
- 一個 `UpdateUI` 規則綁定喺擁有者嘅處理器鏈外一個視窗：列保持 `wxMenuItem` 嘅儲存啟用狀態。通過相同擁有者本地呼叫使用（主幀對於食堂選單）。
- 一個標籤被詞彙層重寫冇目錄條目：列落回到主要標籤單獨。

## 安全考慮

視窗評估搜尋模式通過共用有界正則引擎；冇選單文字或查詢離開過程。

## 驗證

每個選單都提供搜尋嘅更新已寫入整合候選版本，但英文、廣東話、雙語、主題、實際顯示比例同最小尺寸嘅已安裝介面驗證仍然待完成。以下舊畫面唔代表新候選版本或新 Escape 行為已通過驗證。

- `tests/md3_menu`（Catch2）：標籤同埋快捷鍵拆分、快照種類同埋 `UpdateUI` 啟用狀態、篩選分隔符摺疊同埋子選單保留、`place_root` 絕唔交叉錨或離開顯示喺所有四個角、`place_submenu` 喺右邊邊緣翻轉。最後執行：92 assertions 喺 7 測試案例、全部通過。
- 隱藏桌面捕獲來自構建有效載荷：`docs/screenshots/md3-everything/topbar-file-menu--en-light-comfortable--after.png`（檔案選單同埋搜尋丸形同埋快捷鍵欄）、`topbar-file-menu-filtered--…`（輸入 `exp` 留下輸出列）、`ink-row-menu--…`（墨水 ⋯ 選單同埋刪除、分解顏色同埋與...合併禁用對一個單一墨水）。
- 未由驅動驗證：鍵盤導航同埋隱藏桌面上 Escape。便宜無頭鍵路由唔到達彈出（同樣幫助器限制記錄為 `Ctrl+Shift+F`）；鍵處理由代碼審查同埋單位測試涵蓋。

## 建議文章

- [正則構建器](regex-builder.md) 、 選單搜尋丸形後面嘅構建器。
- [外觀自訂](appearance-customization.md) 、 表面令牌來自何處。
- [命令調色板](command-palette.md) 、 其他可搜尋命令表面。
