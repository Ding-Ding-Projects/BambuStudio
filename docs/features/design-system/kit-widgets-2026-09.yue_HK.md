---
translation-of: kit-widgets-2026-09.md
source-sha256: bd278fcf9aebc490deaa8bd7f7264996416ff9472bb7408754c494dc9f806e8d
review-status: agent-drafted
---

> 英文原文：[Kit widgets added in the every-element sweep (2026-09-05)](kit-widgets-2026-09.md)

# 喺每元素掃過中添加嘅工具組件（2026-09-05）

四個工具組件同一個工具組件延伸到達所以冇庫存 wxWidgets 控制必須保留喺一個用戶面向表面。每個鏡像一個現有工具原語所以呼叫者需要冇新詞彙，同每個係釘住由一個原始碼讀取合同喺 `ui-md3/tests/md3-conversion-contracts.test.mjs`。

## LabeledRadioButton 同 RadioGroup（`src/slic3r/GUI/Widgets/LabeledRadioButton.{hpp,cpp}`）

**行為。** 一個可聚焦列嘅繪製 `RadioBox` 字形加一個 `Label`。點擊字形、標籤或列、或按 Space / Enter 當列有焦點時，選擇佢同排放 `wxEVT_RADIOBUTTON` 從列（每個用戶啟動，似乎原生 MSW 控制）。一列喺佢自己可以選擇但唔會由用戶取消選擇。`RadioGroup` 係一個純事件處理器對話框擁有：`Add()` 成員，同佢強制相互排除、隨著上 / 左、下 / 右、主頁同結束移動選擇同焦點、跳過隱藏或禁用成員，同報告 `GetSelection() == -1` 當冇嘢被選擇。`SetSelection(-1)` 清除列以程式方式。

**無障礙。** 列帶著一個 `wxAccessible` 對等物：角色 `ROLE_SYSTEM_RADIOBUTTON`、名稱從標籤、狀態可聚焦 / 聚焦 / 已檢查 / 不可用 / 不可見、預設行動「選擇」。一個 2 px 初級焦點環係繪製圍繞字形。字形本身係唔可聚焦。

**配置。** `SetColorScheme()` 將選擇點重新著色到預覽或設備重點；`Rescale()` 喺 DPI 改變；`SetLabel("")` 隱藏標籤用於純字形列。

**失敗模式。** 一列添加到兩個組被第二個 `Add()` 忽視。摧毀成員從其組移除佢。程式方式 `SetValue()` 永遠唔排放。

**網站。** FeedDirectionDialog、CalibrationWizardPresetPage（階段對同每個位置選擇器）、SavePresetDialog。

## TextArea（`src/slic3r/GUI/Widgets/TextArea.{hpp,cpp}`）

**行為。** 工具組件多行欄位：一個邊框容器裝著一個無邊界原生編輯器通過 `GetTextCtrl()` 到達。OutlineVariant 1 px 喺休息、初級 2 px 當編輯器有焦點、`radius_tiny` 角、SurfaceContainerLowest 填充當可編輯同 SurfaceContainerLow 當唯讀。`SetMonospace(true)` 交換編輯器到 Roboto Mono 用於 JSON、日誌同腳本。`SetMinLines()` 驅動最佳尺寸當父級唔尺寸欄位。

**失敗模式。** 呼叫者需要 `SetStyle()` 或 `wxTE_RICH` 保持用佢哋喺內部控制上。容器唔滾動；編輯器做。

**網站。** 更新對話框更改日誌、訊息對話框腳本主體、系統資訊 JSON、網絡測試日誌、打印評分評論、兩個 WebViewDialog 開發者查看器、未儲存改變差異單元格。

## ListBox（`src/slic3r/GUI/Widgets/ListBox.{hpp,cpp}`）

**行為。** 一個所有者繪製 `wxVListBox` 繪製帶著下拉選單解剖：SurfaceContainer 欄位、圓形 SurfaceContainerHigh 懸停窗格、SecondaryContainer 選擇活動計劃中嘅窗格、OnSurface 文字喺工具組件主體面孔。原生鍵盤模型同 `wxEVT_LISTBOX` 被繼承。長列省略喺末尾同懸停列暴露其完整文字作為提示。

**網站。** 智能家庭實體清單。

## Button::SetIconBitmap 同 ScalableBitmap(wxWindow*, wxBitmap)

`ScalableBitmap` 可以包裹一個位圖呼叫者已經渲染（一個顏色樣本）；佢冇圖示名稱同 `msw_rescale()` 讓佢單獨。`Button::SetIconBitmap()` 展示這樣一個位圖喺每個狀態，所以顏色樣本同其他資料影像可以住喺工具組件圖示按鈕內冇被誤認為圖示資源。

## Button 預設係 Material

一個 `Button` 達到其第一次繪製帶著既無 `SetVariant()` / `SetIconButton()` 也無呼叫者樣式採用 Outlined 變體。每個明確樣式設定者（背景、邊框、文字顏色、角落半徑）標記 Button 呼叫者樣式，所以手工樣式按鈕保持佢哋外觀。Outlined 同 Text 變體定義一個已檢查（選擇）狀態：SecondaryContainer 填充、OnSecondaryContainer 標籤。

## 驗證

- `node --test ui-md3/tests/md3-conversion-contracts.test.mjs` 釘住每個組件嘅登錄、解剖、無障礙角色同對應庫存控制允許清單嘅空性。
- 執行時捕獲（淡同深、EN / 粵語 / 雙語、100 到 200 百分比）係記錄喺 `docs/screenshots/md3-everything/` 一旦構建工藝存在；直到那時 `md3-parity-register.md` 中嘅列係說。

## 建議文章

- [MD3 設計系統](md3-design-system.md)
- [MD3 奇偶校驗登錄](md3-parity-register.md)
- [主題表面顏色](themed-surface-colors.md)
