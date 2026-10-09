---
translation-of: collapsible-filters.md
source-sha256: 59e06015c8f4442e02d454c70ac460cbb696bff02278a2a7bb7382306fb52af6
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
- 表面 ID 會變成設定鍵：第一個字元係細楷字母，之後係細楷字母、數字、`_`、`.` 或者 `-`，最多 64 個字元。其他 ID 一樣收埋到，但係唔會儲存。
- 每個而家排除緊項目嘅篩選條件都要經 `SetActiveFilters()` 報告。統計就留空。

## 測試

`tests/collapsible_filters/collapsible_filters_tests.cpp` 會檢查一開始嘅狀態、模擬重新開啟之後嘅儲存、唔認得嘅儲存值點樣退返預設、唔安全嘅表面 ID 會被拒絕，同埋收埋咗嘅篩選列一定會講明生效中嘅篩選條件，包括冇名嗰啲。

## 仲要喺已建置應用程式度驗證

焦點框睇唔睇到、螢幕閱讀器讀唔讀出，同埋切換之後嘅版面，都要喺已建置嘅 Windows 應用程式度觀察。
