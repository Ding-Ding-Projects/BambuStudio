---
translation-of: sidebar-search.md
source-sha256: 05b22b3d61217ad3862d7feb97d75e4a35bc375dca1a5cebd437e73c4c1beaef
review-status: agent-drafted
---

> 英文原文：[Prepare sidebar search](sidebar-search.md)

# 準備側邊欄搜尋

兩個共享 MD3 `SearchField` 藥丸喺準備左側邊欄，補充現有嘅對象搜尋欄同設定標籤放大鏡。兩個都係 40 px 體育場藥丸帶工具解剖：領先 `search` 字形、`.*` 正則模式切換、同 `tune` 按鈕打開完整引導正則構建器彈窗（原始模式編輯器、令牌部分、標誌、示例文字測試器）。喺 `src/slic3r/GUI/Plater.cpp`（`Sidebar` / `Sidebar::priv`）中實現，重用 `src/slic3r/GUI/Widgets/SearchField.{hpp,cpp}` 同 `src/slic3r/GUI/Search.{hpp,cpp}` 中嘅全球選項索引。

## 設定搜尋（處理卡，涵蓋處理、打印機同絲材選項）

- 一個 `SearchField` 坐喺緊湊處理卡嘅頂部，喺「處理」部分標題下（佔位符：*搜尋設定*）。
- 聚焦欄打開全球 `Search::OptionsSearcher` 結果彈窗，同設定標籤放大鏡使用嘅同一個 `SearchDialog`，直接錨定喺藥丸下。
  鍵入過濾實時（預設模糊/子字串，當啟用正則模式時的有界 Boost.Regex 1.84
  寬字符 ECMAScript；
  無效或半打字模式絕唔會隱藏結果）。
- 範圍係 `Preset::TYPE_INVALID`，即 **搜尋索引嘅每個預設類型用於
  目前模式**：處理/打印、**打印機** 同絲材選項。呢嘅係為什麼打印機部分唔帶第三搜尋欄嘅原因，佢嘅選項係可達自呢個欄。
- 啟動結果（點擊，或 Enter/方向鍵，行係鍵盤可聚焦嘅）發佈 `wxCUSTOMEVT_JUMP_TO_OPTION`；`Sidebar::jump_to_option` 然後翻轉側邊欄到打印選項嘅進階設定（暫時，而唔係持久化用戶嘅緊湊/進階選擇）或啟動擁有嘅打印機/絲材標籤選項。
- 藥丸嘅 `.*` 切換同構建器彈窗嘅區分大小寫 / 完整詞
  複選框被連接到搜尋器嘅持久標誌，通過 `SearchDialog`。

## 絲材槽搜尋（絲材部分）

- 一個緊湊 `SearchField` 坐喺可摺疊絲材區域包裹內，喺「絲材」部分標題下同槽行上方（佔位符：*搜尋絲材*）。
- 當你鍵入時實時過濾可見絲材槽行。每行草垛係 `"<slot number> <preset name> #RRGGBB <nearest colour name>"`，使用 `SearchField::colorSearchText`，同對象搜尋相同語義，所以 `PETG`、`2`、`#00AE42` 或 `green` 都匹配。
- 匹配係 `SearchField::textMatches`：預設子字串，通過 `.*` 切換嘅正則，尊重構建器嘅區分大小寫 / 完整詞標誌。空白查詢顯示每行；非匹配行係被 `Hide()`-den，同物理滾動區域被重新測量（`Sidebar::recalc_filament_scroll_sizes`，它喺每個添加/移除/重縮放路徑上重新應用過濾器，所以新添加或重命名嘅槽保持正確過濾）。
- 過濾器都會重新評估當槽嘅預設或徽章改變（`update_filament_row_badges`），所以重命名同顏色編輯更新結果集。

## 正則構建器訪問

兩個藥丸都配備強制正則構建器：點擊任一欄嘅尾部區域中嘅 `tune` 圖示，打開引導構建器彈窗（字面、字符類、錨、組、交替、量詞、標誌、實時匹配/捕獲測試、複製）。
`.*` 藥丸切換正則模式；純文字搜尋係預設。

## 佈局、主題、DPI

- 兩個欄都坐喺佢哋部分嘅可摺疊面板內（處理卡 / 絲材區域包裝），所以摺疊部分隱藏佢哋。
- 顏色同半徑喺每個繪製時實時從 MD3 令牌解析，暗模式唔需要額外配線。`Sidebar::msw_rescale` 對兩個藥丸呼叫 `Rescale()`。

## 失敗模式

- 無效 / 半打字正則：喺兩個表面中匹配一切（冇行被隱藏）。
- 災難反向跟蹤模式：捕獲咗（`std::regex_error`）同視為匹配全部；選項搜尋器都限制模式長度。
- 所有絲材行被過濾掉：槽列表摺疊到空；清除查詢（藥丸嘅清除按鈕）恢復每行。

## 驗證

1. 構建同打開帶多絲材項目嘅準備。
2. 處理卡：點擊 *搜尋設定* 藥丸，鍵入 `wall`，結果列表整個處理/打印機/絲材嘅選項帶類型標記；點擊一個同確認跳轉（打印選項翻轉側邊欄到進階設定；打印機選項打開打印機標籤）。
3. 鍵入打印機專用選項（例如 `nozzle diameter`）同確認打印機標籤跳轉。
4. 絲材部分：鍵入預設子字串、槽號、顏色名稱（`green`）同十六進制值（`#RRGGBB` 的槽），只有匹配行保持；清除恢復所有。
5. 切換 `.*` 同使用模式（例如 `PLA|PETG`）；打開 `tune` 構建器同驗證標誌重新執行兩個過濾器實時。
6. 摺疊每個部分，藥丸隱藏帶佢哋嘅部分。改變 DPI/主題，藥丸重新衍生幾何同顏色。
7. 目錄：嚟自 `bbl/i18n/yue_HK`，`compile_translation.py` 同 `compile_translation.py --check` 都通過（620 條訊息）。
