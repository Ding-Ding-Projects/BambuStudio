---
translation-of: native-controls.md
source-sha256: cfa0832ec813fb7b518a12f45e78c8e11b99ee8525d4fca153616fe43816e87c
review-status: agent-drafted
---

> 英文原文：[Native controls on the kit](native-controls.md)

# 套件上嘅原生控件

有幾個 Windows 原生控件仲喺大家會用到嘅介面度：停用咗嘅掣嘅提示、網頁上面嘅通知列、工作區面板嘅分頁、清單、待辦清單同月曆、
每個分組框、「打印床形狀」嘅頁選擇器、三個對話框嘅「確定」同「取消」掣，同埋每個會捲動嘅頁面、面板、清單、表格、多行文字框同 HTML 檢視嘅捲動列。佢哋用系統
字型畫出 Windows 嘅樣，唔理主題係乜。而家每個都有對應嘅套件控件。

## 邊個換咗邊個

| 原生 | 套件 | 喺邊度 |
| --- | --- | --- |
| `wxTipWindow` | `ButtonDisabledTip`：Material 純文字工具提示 | 停用咗嘅套件掣嘅提示；系統唔會幫停用咗嘅視窗顯示提示 |
| `wxInfoBar` | `MD3InfoBanner`：Material 橫額 | 雲端網頁載入失敗時喺網頁上面顯示嘅通知，附「重試」 |
| `wxNotebook` | 喺 `wxSimplebook` 上面嘅 `TextTabbar` | 工作區面板嘅分頁 |
| `wxListCtrl` | 用 Material 表格樣式嘅 `wxDataViewListCtrl` | 工作區面板嘅成員清單同日曆議程 |
| `wxCheckListBox` | 加咗剔選框嘅套件 `ListBox` | 工作區待辦清單 |
| `wxCalendarCtrl` | 用 Material 顏色嘅 `wxGenericCalendarCtrl` | 工作區日曆 |
| `wxStaticBox`（分組框） | `MD3GroupBox`：Material 外框同標題 | 設定分頁以外嘅選項組、「打印床形狀」對話框、校準精靈頁、「儲存預設」、未儲存變更嘅比較、墨水揀選器嘅預覽 |
| `wxChoicebook` | 喺 `wxSimplebook` 上面嘅套件 `ComboBox` | 「打印床形狀」對話框嘅形狀 |
| `CreateButtonSizer()`、`CreateStdDialogButtonSizer()` | 用標準 id 嘅套件掣 | 「打印床形狀」、「系統資訊」、未儲存變更嘅完整比較 |
| `wxScrolledWindow`、套件 `ListBox`、每個表格（`wxDataViewCtrl`、`wxDataViewListCtrl`）、每個多行文字框同每個 HTML 檢視（`wxHtmlWindow`）嘅 Windows 捲動列 | `MD3ScrolledWindow`、套件 `ListBox`、`MD3DataViewCtrl`、`MD3DataViewListCtrl`、`TextAreaEditor` 同 `MD3HtmlWindow`，全部都畫套件捲動列（`MD3ScrollBars`） | 每個會捲動嘅頁面同面板：準備側邊欄、每個設定頁、每個偏好設定頁、「裝置」分頁、指令面板、長嘅訊息框同對話框；套件清單；每個表格：物件清單、專案檔案清單、未儲存變更嘅比較、工作區清單、「設定檔同備份」、「匯出」、「版本記錄」、「通知中心」、打印主機佇列；同埋每個多行文字框：`TextArea`（更新說明、記錄、指令碼、筆記、提示）、設定嘅 G-code 欄位，同正規表示式建立器嘅範例同結果；同埋每個 HTML 檢視：有表格或者連結嘅訊息框內文、系統資訊、關於、設定精靈嘅 HTML 頁面同其他 HTML 說明 |

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
- **分組框。** `MD3GroupBox` 係一個自己畫邊框帶嘅 `wxStaticBox`：1 px OutlineVariant 小圓角外框，標題用套件細標題字型、
  OnSurface 色（停用時用 OnSurfaceVariant）。`wxStaticBoxSizer` 依賴嘅嘢仍然由原生框負責，所以入面啲控件排版同以前一樣。
  側邊欄嘅擠出機分組（`StaticGroup`）改用 OutlineVariant 同 OnSurfaceVariant，唔再用舊灰色。
- **打印床形狀。** 套件下拉選單負責揀 `wxSimplebook` 嘅頁，唔再用喺頁上面放原生選擇控件嘅 `wxChoicebook`。對話框、佢啲頁同掣
  唔再有固定白色，搵唔到嘅紋理或者模型檔案用 Error 角色標示，唔再用原始紅色。
- **「確定」同「取消」。** 標準掣排列會整出原生掣。改用標準 id 嘅套件掣（Filled「確定」、Outlined「取消」），所以對話框自己嘅
  「確定」、「取消」同 Escape 處理照舊有效。
- **捲動列。** Windows 以前喺每個會捲動嘅視窗度，兩個主題都畫一條 17 px 灰色嘅條，入面有一條幼幼嘅灰色滑塊。
  `MD3ScrolledWindow`（以前用 `wxScrolledWindow` 嘅地方都改用佢）、套件 `ListBox`、每個表格用嘅 `MD3DataViewCtrl` 同
  `MD3DataViewListCtrl`，同埋每個多行文字框用嘅 `TextAreaEditor`，開視窗嘅時候唔帶 Windows 捲動列，改為畫套件捲動列：
  一條 10 px 嘅條，顏色同後面嘅表面一樣，入面有一粒完全圓角、四邊縮入 2 px 嘅滑塊，平時係 OutlineVariant，指標停喺上面
  或者拖緊嗰陣係 Outline。捲動列放喺內容旁邊，即係以前 Windows 捲動列嘅位置，所以冇嘢會排喺佢下面；Windows 捲動列多佔
  嘅 7 px 亦都還返畀內容。拖滑塊，內容會跟住郁；撳軌道會向指標嗰邊翻一頁，撳住唔放就會一直翻。滾輪、鍵盤，同埋將有焦點
  嘅欄位捲入畫面，都同以前一樣，因為捲動仍然係 wx 自己嘅捲動邏輯做，佢只係話畀套件捲動列知要畫喺邊。開咗 Windows 高對比
  嘅時候，捲動列用系統嘅視窗同文字顏色。表格自己嘅捲動邏輯照舊捲動佢嘅行同表頭，方向鍵亦照舊移動揀選。多行文字框係一個
  Windows 編輯控件，佢自己會跟住游標、滾輪同鍵盤捲動；每次收到可能移動文字嘅訊息之後，`TextAreaEditor` 都會讀佢嘅第一條
  可見行、總行數同顯示得到嘅行數，照住畫捲動列；拖捲動列或者翻頁嗰陣，就逐行捲動編輯控件。HTML 檢視
  （`wxHtmlWindow`）都係一個會捲動嘅視窗，`MD3HtmlWindow` 畀佢同樣嘅套件捲動列：有表格或者連結嘅訊息框內文、系統資訊、
  關於、設定精靈嘅 HTML 頁面同其他 HTML 說明。

## 邊啲保留原生，點解

- 系統檔案對話框，因為佢哋帶埋用戶常用嘅位置、預覽同 Windows 殼層。
- 冇任何建置會顯示得到嘅原生類別：SLA 壓縮檔匯入嘅檔案揀選器（匯入冇選單項目）、`wxExtensions` 入面嘅剔選清單下拉彈出
  （冇人呼叫），同從來冇建構過嘅監察基礎面板同佢嘅分割器。

## 對話框標題列跟住標題行

套件標題列（`MD3DialogCaption`）取代咗原生標題列，但係佢淨係喺 `Adopt` 嗰刻讀一次對話框嘅標題。對話框之後再叫
`SetTitle`，改到嘅只係無框視窗下冇人見到嘅視窗文字，標題列照舊顯示第一個標題：「裝載到噴嘴」對話框（`FeedDirectionDialog`）
本應寫 `Load <tray> to left nozzle`，但係一直寫住 `Confirm`；`DeviceErrorDialog`、`ParamsDialog` 同 `ExtrusionCalibration`
都係咁樣改標題。由 `ad910deb2` 起，由對話框自己嘅標題採用嘅標題列（`Adopt` 唔傳字串）會跟住個標題行：閒置時將對話框標題
同上次套用嘅標題比較（唔係同標籤文字比較，因為雙語裝飾器可能已經配對咗佢），唔同就更新標籤同無障礙名稱。
`MD3DialogCaption::SyncTitle(dialog)` 可以即時喺同一次繪製套用改變；「裝載到噴嘴」對話框每次 `SetTitle` 之後都會叫佢。
採用時傳咗字串嘅標題列係特登揀嘅標題，照舊唔郁。

## 驗證

- `node --test ui-md3/tests/native-controls.test.mjs` 會拒絕 GUI 入面任何地方嘅原生分頁控件、報告清單、待辦清單、月曆、提示視窗、
  通知列、分組框、選擇頁簿、樹狀控件或者標準掣排列，同埋檢查每個套件替代品。
- `node --test ui-md3/tests/scrollbars.test.mjs` 會拒絕新嘅 `wxScrolledWindow`、`wxHtmlWindow`、資料檢視表格、資料檢視清單或者多行
  `wxTextCtrl`，檢查套件捲動列從來唔會將捲動列交畀 Windows、會喺非客戶端區域預留自己條位，同埋以前幫 Windows 捲動列
  留位嘅程式碼而家幫套件捲動列留位。
- 版面探針會幫每個視窗記低佢顯示緊 Windows 捲動列定係套件捲動列（`scrollbars`：`native_v`、`native_h`、`kit_v`、`kit_h`），
  所以一個發佈版本嘅探針轉儲會列出畫面上仲有嘅每一條 Windows 捲動列。
- `node --test ui-md3/tests/dialog-caption-title-sync.test.mjs` 會檢查由對話框標題採用嘅標題列會跟住佢行、`SyncTitle` 會去到
  對話框每一個標題列，同埋「裝載到噴嘴」對話框採用時唔傳字串、每次 `SetTitle` 之後都會同步。
- 仲未喺發佈版本度逐個用過呢啲介面；標題列嘅改動要有雙噴嘴打印機嘅裝載對話框先睇到。
