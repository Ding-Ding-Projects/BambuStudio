---
translation-of: graphite-shell.md
source-sha256: b334e445f213d39b83dc2d55a30c94dd44676ea7a14e4349776ab4525c2b0c5c
review-status: agent-drafted
---

# 石墨外殼

石墨外殼係主視窗外框嘅重新設計。佢改咗工作區擺放嘅位置，同埋所有 Material Design 3 表面用嚟上色嘅中性色盤。切片、打印、預設、專案同呢個分支嘅所有功能都冇改。

## 改咗乜

- **導航欄。** 主工作區（`MainFrame::m_tabpanel`）用 `wxNB_LEFT` 靠左擺。`ButtonsListCtrl` 將各個目的地排成 88 px 闊嘅直欄：每個目的地係 72 x 60 px 嘅方塊，Material Symbols 圖示喺標籤上面；揀咗嘅方塊用次要容器色，前緣有一條短嘅主色條，唔使靠顏色都認得出。導航動作（設定）固定喺最底。視窗太矮嗰陣，尾段會好似以前橫條咁收入溢出選單，用同一個 `NotebookNavigation::fit()`，不過係量高度。
- **其他分頁簿保持頂部橫條。** 冇指定停靠方向嘅分頁簿仍然預設 `wxBK_TOP`，所以設定對話框同佢嘅子分頁冇郁過。
- **石墨色盤。** `MD3Tokens.hpp` 入面嘅中性色階變成帶少少綠嘅石墨色。深色：底色 `#0e1311`，容器由 `#0a0e0c` 到 `#28322c`，文字 `#e3ebe5` 同 `#a9b6ad`，主色 `#6fdc98`。淺色：底色 `#f5f8f5`，容器由 `#ffffff` 到 `#d6e1da`，文字 `#131c17` 同 `#3f4c44`，主色 `#146c2e`。兩個主題入面，文字對每個表面都有至少 4.5:1 對比，外框至少 3:1，深色兼容對照表仍然係冪等（`tests/native_navigation/neutral_palette.test.mjs`）。
- **浮動畫布。** 3D 畫布（準備、預覽同組裝）而家喺視窗底色上面，四邊各留一個密度間距，側欄同畫布之間嗰條細線拎走咗。因為 OpenGL 視圖係原生子視窗，畫布角仍然係直角。
- **浮動側欄卡片。** 準備頁側欄嘅各個部分（打印機、耗材、處理、物件清單同物件操作）而家係放喺視窗底色上面嘅圓角卡片，唔再由一邊頂到另一邊。每張卡用 SurfaceContainerLow，加 1 px OutlineVariant 邊同 24 px 面板圓角，卡入面留 8 px，離側欄邊半個密度間距。只會畫目前分頁嗰部分嘅卡，打印機同耗材之間嗰條分隔線拎走咗。各部分保留自己嘅控件同顏色。
- **標題列語言切換。** 視窗控制掣前面有一個三段切換（EN、粵、雙），顯示而家嘅語言模式。揀另一段會行同偏好設定一樣嘅步驟：有未儲存專案就問你要唔要儲存、確認要重新啟動、檢查改咗嘅預設，然後用新模式重建介面。如果用緊其他已安裝語言，就冇一段會被標示。
- **標題列更新晶片。** 背景下載好一個更新、等緊安裝嗰陣，語言切換後面會出現一粒主要容器色嘅「重新啟動以更新」膠囊。㩒落去會用同通知入面重新啟動連結一樣嘅路徑，重新啟動入新版本。佢嘅提示寫明更新冇數碼簽署。其他時間佢都會收埋，通知照舊會出。
- **網頁跟住色盤。** 每個自帶一份顏色角色嘅內置網頁（主頁、裝置頁、耗材管理同建立、指南、登入同模型頁）而家都用石墨數值，網頁表面同原生表面睇落一致。
- **主頁。** 主頁唔再用漸變橫幅，改為放喺底色上面嘅大標題，配 16 px 圓角按鈕；最近專案喺一個 24 px 面板入面排成三欄，最多六個。以下截圖係用 Chromium 打開隨附嘅網頁再填入示範最近檔案，唔係由運行中嘅應用程式截取：

  ![主頁，深色](../../screenshots/graphite-shell/home--en-dark--browser-render.png)
  ![主頁，淺色](../../screenshots/graphite-shell/home--en-light--browser-render.png)
- **新設計記號。** `navigation_rail_width`、`navigation_rail_item_width`、`navigation_rail_item_height` 同 `radius_panel`（24 px，用喺浮動面板同 3D 畫布外框）。

## 驗證

- 喺 Linux 用 wxWidgets 3.2、Boost 同 OpenCASCADE 標頭做 `g++ -fsyntax-only`，`Notebook.cpp` 同 `MainFrame.cpp` 都通過。
- `tests/native_navigation/graphite_rail.test.mjs` 鎖定靠左停靠、其他分頁簿預設頂部橫條、導航欄尺寸同石墨底色。
- `tests/native_navigation/graphite_sidebar_cards.test.mjs` 鎖定卡片留白同圓角嘅關係、喺加齊所有部分之後先設定一次、每次轉部分都重新排版，同埋顏色角色。
- `tests/native_navigation/graphite_title_bar.test.mjs` 鎖定語言切換嘅模式、標題列項目嘅次序、未準備好就收埋嘅更新晶片同佢嘅重新啟動動作。
- Windows 託管建置已經編譯咗導航欄、色盤、浮動畫布同語言切換。
- node 合約測試套件嘅失敗同 `main` 一樣，冇新增。`tests/native_shared_controls/atlas_shell_preserved_functions.json` 入面 `MainFrame::init_tabpanel` 嘅指紋已經更新，因為今次改動係刻意改嗰個函數。

未驗證：更新晶片同側欄卡片嘅 Windows 託管建置、實際運行嘅應用程式、英文、廣東話同雙語模式喺各個縮放比例嘅截圖，同埋喺導航欄入面用鍵盤移動嘅次序。

## 下一步

設計仲將畫布外框改成圓角，喺主頁加打印機同區域網絡模型投遞一欄，同埋重新設計預覽、裝置同各個對話框。呢啲仲未做。完整設計喺 `design/graphite-shell/`。
