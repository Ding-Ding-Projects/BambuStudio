---
translation-of: native-controls.md
source-sha256: e39d1d16b013effc7e287fee7f9409e2f26beb14a09859d0f8e22b4d8d11e28b
review-status: agent-drafted
---

> 英文原文：[Native controls on the kit](native-controls.md)

# 套件上嘅原生控件

有幾個 Windows 原生控件仲喺大家會用到嘅介面度：停用咗嘅掣嘅提示、網頁上面嘅通知列，同工作區面板嘅分頁、清單、待辦清單同月曆。
佢哋用系統字型畫出 Windows 嘅樣，唔理主題係乜。而家每個都有對應嘅套件控件。

## 邊個換咗邊個

| 原生 | 套件 | 喺邊度 |
| --- | --- | --- |
| `wxTipWindow` | `ButtonDisabledTip`：Material 純文字工具提示 | 停用咗嘅套件掣嘅提示；系統唔會幫停用咗嘅視窗顯示提示 |
| `wxInfoBar` | `MD3InfoBanner`：Material 橫額 | 雲端網頁載入失敗時喺網頁上面顯示嘅通知，附「重試」 |
| `wxNotebook` | 喺 `wxSimplebook` 上面嘅 `TextTabbar` | 工作區面板嘅分頁 |
| `wxListCtrl` | 用 Material 表格樣式嘅 `wxDataViewListCtrl` | 工作區面板嘅成員清單同日曆議程 |
| `wxCheckListBox` | 加咗剔選框嘅套件 `ListBox` | 工作區待辦清單 |
| `wxCalendarCtrl` | 用 Material 顏色嘅 `wxGenericCalendarCtrl` | 工作區日曆 |

## 點做

- **停用咗嘅掣嘅提示。** 停用咗嘅視窗冇系統工具提示，所以套件 Button 用自己嘅彈出視窗顯示提示。佢同其他所有工具提示一樣係
  Material 純文字工具提示：InverseSurface 底配 InverseOn 字、套件嘅細字型、8 x 4 DIP 留白，喺 Windows 11 仲有細圓角。
  佢喺指標下面打開，唔會走出嗰個螢幕，亦永遠唔會攞走指標或者焦點。
- **網頁橫額。** `MD3InfoBanner` 係一條 SurfaceContainerHigh 長條，有狀態圖示（Warning 用 Error 角色，Info 用 Primary）、
  用套件內文字型嘅訊息、做動作嘅套件文字掣同關閉掣，底部有一條 OutlineVariant 邊線。佢保留咗網頁面板用開嘅嘢：
  `ShowMessage()`、`Dismiss()`，同一個會將 `wxEVT_BUTTON` 連佢嘅 id 送去面板嘅動作。
- **工作區分頁。** 套件 `TextTabbar` 負責切換 `wxSimplebook` 嘅頁。分頁列而家喺每個出現嘅地方（工作區面板同預設比較）都用
  Material 角色：佢所在嘅表面、OutlineVariant 分隔線，同用 Primary 嘅現用分頁標籤同指示條。
- **表格。** `wxExtensions` 入面嘅 `md3_style_data_view()` 令 `wxDataViewCtrl` 用套件內文字型，OnSurface 字配
  SurfaceContainerLowest 底；32 DIP 列高，每隔一行加 SurfaceContainerLow 間紋；表頭用套件細標題字型同 OnSurfaceVariant。
  工作區清單、「設定檔同備份」、「匯出」、「版本記錄」同「通知中心」都用佢。
- **待辦清單。** `ListBox::EnableChecks()` 喺每行開頭畫 Material 剔選框圖示。撳圖示或者撳 Space 鍵會切換佢，並且好似原生
  待辦清單咁送出帶行號嘅 `wxEVT_CHECKLISTBOX`；撳行上面其他地方就揀中嗰行，方便用「編輯」、「上移」同「下移」。
- **日曆。** 通用日曆用畀佢嘅顏色自己畫：SurfaceContainerLowest 底配 OnSurface 日子，星期表頭用 OnSurfaceVariant，揀中嘅日子
  用 Primary。用逐月切換嘅樣式時，佢會自己畫帶箭嘴嘅月份表頭，唔再用原生下拉選單同數值控件。

## 邊啲保留原生，點解

- 系統檔案對話框，因為佢哋帶埋用戶常用嘅位置、預覽同 Windows 殼層。
- 冇任何建置會顯示得到嘅原生類別：SLA 壓縮檔匯入嘅檔案揀選器（匯入冇選單項目）、`wxExtensions` 入面嘅剔選清單下拉彈出
  （冇人呼叫），同從來冇建構過嘅監察基礎面板同佢嘅分割器。

## 驗證

- `node --test ui-md3/tests/native-controls.test.mjs` 會拒絕 GUI 入面任何地方嘅原生分頁控件、報告清單、待辦清單、月曆、提示視窗
  或者通知列，同埋檢查每個套件替代品。喺之前嘅源碼上六個情況全部失敗。
- 仲未喺發佈版本度逐個用過呢啲介面。
