---
translation-of: themed-surface-colors.md
source-sha256: 5cf8393a660db723ee426cfbb3cbd184936cc22e4814cbab848543c79a6f7fe9
review-status: agent-drafted
---

> 英文原文：[Themed surface colors on StaticBox cards](themed-surface-colors.md)

# StaticBox 卡片上嘅主題表面顏色

主題卡片實際如何獲得其填充，以及造成卡片喺深色模式下呈現 **淺色打印板** 嘅兩個陷阱，即使傳送給佢們嘅每個顏色都係正確嘅。

## 行為

`StaticBox`（`src/slic3r/GUI/Widgets/StaticBox.cpp`）係對話框、橫幅和自訂 `Button` 後面嘅卡片／打印板原始物。佢喺 `doRender()` 中使用兩個 `StateColor` 繪製自身：

- `border_color`：喺構造函式中用 Outline／OutlineVariant 播種。
- `background_color`：**空直到某個項目設定佢。**

表面通過呼叫 `SetBackgroundColorNormal(colour)` 應用其主題（通常來自也重新播種子 `Label` 背景嘅 `apply_theme()`）。

## 故障模式

### 1. `setColorForStates()` 係更新，而唔係插入

```cpp
bool StateColor::setColorForStates(wxColour const &color, int states)
{
    for (...) if (statesList_[i] == states) { colors_[i] = color; return true; }
    return false;   // no entry for this state -> nothing happens
}
```

因為 `background_color` 開始為空，`SetBackgroundColorNormal()` 係每個卡片上嘅 **無聲無操作，永遠冇明確 `SetBackgroundColor()`**。`doRender()` 然後採用其 `background_color.count() == 0` 後援並用 `GetBackgroundColour()` 填充，普通 `wxWindow` 顏色。`SetBackgroundColorNormal()` 現在喺正常狀態顏色缺失時插入佢，只有然後回退到就地更新。

### 2. 普通視窗背景變得陳舊

`StaticBox::Create()` 播種 `wxWindow` 自身背景一次，來自父項，通過 `GetParentBackgroundColor()`。因此喺應用主題 **之前** 構造嘅任何卡片都快取淺色表面。該顏色唔係裝飾嘅：MSW `render()` 路徑喺 `doRender()` 繪製前用 `GetBackgroundColour()` 清除其後台緩衝區，預設擦除路徑也使用佢。`SyncWindowBackground()` 現在喺每個 `SetBackgroundColor`／`SetBackgroundColorNormal` 上從主題正常狀態顏色重新播種佢。

兩個修復都喺小工具中，因此應用中嘅每個主題卡片都被涵蓋，唔僅係首次報告症狀嘅表面。

## 安全考量

無；呢個係無 I/O、無用戶資料和無組態嘅呈現層顏色路徑。

## 驗證

通過 `.claude/skills/run-bambustudio/` 喺實際構建二進位上重現和確認（無頭 Mesa llvmpipe + `PrintWindow`），喺深色模式，喺版本歷史對話框上：

- **之前**：[`history-dialog-dark--before-card-fix.png`](../../screenshots/version-history/history-dialog-dark--before-card-fix.png)：
  卡片內部樣本 `#F0F0F0`，而佢們頂部嘅標籤正確樣本 `#202127`。
- **之後**：[`history-dialog-dark.png`](../../screenshots/version-history/history-dialog-dark.png)：
  卡片內部樣本 `#202127`，匹配佢們嘅標籤；所有五個標籤可讀。

像素樣本（x = 400、訊息卡片）係診斷，將「標籤錯誤」從「標籤下嘅打印板錯誤」分開，兩者喺縮圖中看起來相同。

> [!NOTE]
> `HANDOFF.md` 中嘅早期診斷將此歸因於 `Label` 喺構造時快取其父背景。呢個係真實陷阱，`apply_theme()` 確實為其重新播種標籤背景，但佢係 **唔係** 此症狀嘅原因：標籤已經正確繪製。
