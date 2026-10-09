---
translation-of: collapsible-filters.md
source-sha256: f52d57873e6f7b94da648b8d3f0169759cefa41c367ae329fbb49aa454a601f5
review-status: agent-drafted
---

> 英文原文：[Collapsible filters and statistics](collapsible-filters.md)

# 可收埋嘅篩選同統計

搜尋列、篩選列同統計面板都可以收埋，等佢哋所屬嘅清單或者畫面有多啲位。每一個都喺一個有箭嘴嘅細標題下面；撳個標題就可以收埋或者顯示下面嘅控制項。

## 行為

- **一開始嘅狀態。** 會改變集合顯示內容嘅控制項（搜尋列、篩選晶片、日期或者類別篩選）一開始係展開嘅。只係描述集合嘅面板（數目、總數、統計）一開始係收埋嘅。
- **會記住。** 每個表面嘅選擇都會儲存，重新開啟之後會還原。佢存喺 `BambuStudio.conf` 嘅 `collapsible_filters` 部分，每個表面一個 `expanded` 或者 `collapsed` 值。如果個值被改過或者唔認得，就會用返上面嘅一開始狀態。
- **唔會靜靜雞收埋嘢。** 篩選列收埋咗而入面仍然有篩選條件收窄緊清單嘅時候，標題下面會有一行講明，例如 `Active filters (2): Errors · Search: abc`。最多會列出三個篩選條件嘅名，其餘會計數（`+2 more`）。標題本身喺兩種狀態下都會顯示數目，例如 `Search and filters (2 active)`。撳嗰行就會展開。
- **鍵盤。** 標題係 Tab 次序入面一個普通、可以聚焦嘅按鈕。<kbd>Space</kbd> 或者 <kbd>Enter</kbd> 會切換佢，聚焦時會畫共用嘅 Material 焦點框。如果焦點喺入面嘅時候收埋，焦點會移返去標題，唔會停喺一個睇唔到嘅控制項度。
- **螢幕閱讀器。** 標題會報告自己係一個已展開或者已收埋嘅按鈕，名稱包括生效中篩選條件嘅數目，描述會重複上面嗰行。狀態、數目或者嗰行改變時，會發出對應嘅無障礙變更事件，等輔助技術讀出嚟。預設動作叫 **Expand（展開）** 或者 **Collapse（折疊）**。

## 用喺邊度

每個表面都會記住自己嘅狀態。

| 表面 | 標題 | 收埋時當係生效中嘅篩選 |
|---|---|---|
| 通知中心 | 搜尋同篩選 | 搜尋文字、唔係 **All levels** 嘅級別晶片、隱藏咗已關閉嘅通知 |
| 新功能（更新紀錄） | 搜尋同篩選 | 搜尋文字、**From** 或者 **To** 日期 |
| 項目版本歷史 | 搜尋同篩選 | 搜尋文字、唔係 **All** 嘅類別或者狀態、裝置識別碼、**From** 或者 **To** 日期、Git 圖檢視顯示緊嘅儲存庫 |
| 裝置農場工具列 | 搜尋 | 搜尋文字（標題喺工具列嗰行入面） |
| 設定檔同備份 | 搜尋 | 搜尋文字 |
| 用戶預設 | 搜尋 | 搜尋文字 |
| 打印主機上載佇列 | 搜尋 | 搜尋文字（佇列會選取符合嘅項目，唔會隱藏行） |
| 本地身份歷史 | 搜尋同篩選 | 搜尋文字、**From** 或者 **To** 日期、每個被關閉嘅動作類型 |
| Status Hub | 搜尋 | 搜尋文字 |
| 匯出對話框 | 搜尋 | 搜尋文字 |
| 側邊欄物件搜尋 | 搜尋 | 搜尋文字；標題會同物件清單一齊隱藏同顯示 |
| 側邊欄墨水槽搜尋 | 搜尋 | 搜尋文字；同以前一樣會隨墨水部分一齊收埋 |
| 側邊欄設定搜尋（精簡卡同完整設定列） | 搜尋 | 冇：呢兩個會開結果清單，唔會隱藏設定，所以永遠冇隱藏咗嘅篩選要報告 |

喺側邊欄，<kbd>Ctrl</kbd>+<kbd>F</kbd> 仍然會聚焦物件搜尋。如果搜尋收埋咗，呢個快捷鍵只會喺今次工作階段展開佢；記住咗嘅選擇唔會變。

## 3D 畫布上嘅面板

有兩個面板係喺 3D 畫布上面畫，唔係原生控制項：預覽嘅圖例同統計欄，同埋組裝檢視嘅 **Assembly Structure** 面板。佢哋跟同一套規則：

- **會記住。** 收埋狀態以 `preview_legend` 同 `assembly_structure` 存喺同一個 `collapsible_filters` 部分，重新開啟之後會還原。兩個一開始都係展開：圖例欄有改變預覽顯示內容嘅檢視模式晶片同顯示選項，結構面板係工作中嘅步驟樹。現有嘅 `use_last_fold_state_gcodeview_option_panel` 設定（預設開啟）仍然決定預覽會唔會還原已儲存嘅狀態；關咗嘅話，每次載入新 G-code 都會好似以前咁展開。
- **鍵盤。** 畫布有焦點時，<kbd>Shift</kbd>+<kbd>L</kbd> 會喺預覽收埋或者展開圖例欄，喺組裝檢視收埋或者展開結構面板。單獨 <kbd>L</kbd> 喺預覽保持原本意思（垂直滑桿嘅單層模式）。用咗快捷鍵之後，面板嘅切換掣會顯示 Primary 焦點框；之後喺畫布撳滑鼠就會收返。快捷鍵列喺鍵盤快捷鍵對話框嘅 **Preview** 下面。
- **螢幕閱讀器。** 畫布視窗會將每個面板切換掣公開做一個按鈕，名叫 **Legend and statistics** 或者 **Assembly Structure**，報告已展開或者已收埋，位置就係畫出嚟嘅位置，輔助技術亦可以啟動佢。用快捷鍵之後，切換掣會先收到無障礙焦點，再收到狀態變更事件，所以會讀出新狀態；用滑鼠切換會發出狀態變更事件。

## 俾開發者

共用元件係 `CollapsibleFilterBar`（`src/slic3r/GUI/Widgets/CollapsibleFilterBar.{hpp,cpp}`）。規則放喺唔使工具包嘅 `src/slic3r/GUI/Widgets/CollapsibleFilterState.hpp`，所以唔使 wxWidgets 都測試到。

```cpp
auto *filters = new CollapsibleFilterBar(this, "my_surface", _L("Search and filters"),
                                         CollapsibleFilterBar::Purpose::Narrows);
m_search = new SearchField(filters->GetBody(), _L("Search items"));
filters->GetBodySizer()->Add(m_search, 0, wxEXPAND);
root->Add(filters->GetSectionSizer(), 0, wxEXPAND | wxBOTTOM, gap);

// Whenever the filter changes:
filters->SetActiveFilters({_L("Errors"), wxString::Format(_L("Search: %s"), query)});
```

- 可以收埋嘅控制項要用 `GetBody()` 做父視窗。收埋只會隱藏嗰個面板，所以主介面俾自己控制項嘅顯示或者隱藏狀態永遠唔會被改。
- 統計同摘要用 `Purpose::Describes`，咁佢哋一開始就係收埋嘅。
- 篩選列重畫時會跟返主介面嘅背景色，DPI 改變時亦會自己重新縮放，所以主介面唔使額外處理主題或者縮放（喺現有流程度呼叫 `SyncTheme()` 或者 `Rescale()` 都冇問題）。
- 表面 ID 會變成設定鍵：第一個字元係細楷字母，之後係細楷字母、數字、`_`、`.` 或者 `-`，最多 64 個字元。其他 ID 一樣收埋到，但係唔會儲存。
- 每個而家排除緊項目嘅篩選條件都要經 `SetActiveFilters()` 報告。統計就留空。靜態輔助函數 `SearchFilterLabel()`、`FilterLabel()` 同 `ExcludedFilterLabel()` 會砌出標籤；值會縮短到 32 個字元。
- `Layout::Inline` 會將標題、說明行同控制項放喺同一行，俾工具列用。`ShowSection()` 會同所屬集合一齊隱藏或者顯示成個部分，`SetExpanded(true, false)` 會展開控制項但唔儲存選擇。

畫布面板用 `CollapsibleFilters::CanvasDisclosure` 保存狀態，用 `CanvasDisclosures::publish()` / `announce()`（`src/slic3r/GUI/Widgets/CanvasDisclosures.{hpp,cpp}`）令畫布嘅無障礙物件同 ImGui 畫出嚟嘅嘢保持一致。

## 測試

`tests/collapsible_filters/collapsible_filters_tests.cpp` 會檢查一開始嘅狀態、模擬重新開啟之後嘅儲存、唔認得嘅儲存值點樣退返預設、唔安全嘅表面 ID 會被拒絕，同埋收埋咗嘅篩選列一定會講明生效中嘅篩選條件，包括冇名嗰啲。`collapsible_filters_contract.test.mjs` 會檢查元件將規則接駁到設定、鍵盤同無障礙物件；`collapsible_filters_adoption.test.mjs` 會檢查上面表入面每個表面都喺可收埋嘅主體入面建立控制項、排好個部分，並報告生效中嘅篩選條件。`canvas_panels_contract.test.mjs` 會檢查兩個畫布面板會儲存狀態、回應 <kbd>Shift</kbd>+<kbd>L</kbd>、畫焦點框，同埋公開同讀出佢哋嘅切換；設定 `COLLAPSE_SOURCE_REF` 就會用另一個版本嚟檢查。

## 仲要喺已建置應用程式度驗證

焦點框睇唔睇到、螢幕閱讀器讀唔讀出、上面每個表面切換之後嘅版面、轉淺色或者深色主題之後可收埋主體嘅顏色，同埋狀態喺重新開啟之後仲喺唔喺度，都要喺已建置嘅 Windows 應用程式度觀察。畫布面板方面，仲要觀察預覽同組裝檢視嘅 <kbd>Shift</kbd>+<kbd>L</kbd>、每個顯示縮放比例下焦點框嘅位置，同埋用快捷鍵同用滑鼠切換之後螢幕閱讀器讀出乜嘢。
