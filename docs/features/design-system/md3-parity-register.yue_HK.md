---
translation-of: md3-parity-register.md
source-sha256: bd10792c127b890f771fe5274fda1f91243495428a89a216ec19d00bb0968698
review-status: agent-drafted
---

> 英文原文：[MD3 parity register](md3-parity-register.md)

# MD3 統一登記冊

## 授權

Bambu Studio 嘅全整個 GUI 都必須符合內嵌 `ui-md3/design-system` 套件規格（[`ui-md3/design-system/`](../../../ui-md3/design-system/)、原生 token 鏡像 `src/slic3r/GUI/Widgets/MD3Tokens.hpp`）。目標係**零原創設計元素（舊有 Bambu 設計風格）**：每一個介面、組件、對話框、浮動視窗、圖示同字型都係遷移到套件組件／token，要唔係就明確記錄為刻意嘅偏離。

**功能性資料顏色係獲豁免嘅。** 墨水／AMS 色板顏色、擠出角色同 G-code功能調色盤、工具轉換帶子、軸線顏色、打印機產品相片、品牌標誌、教學圖表同校準結果影像係*資料*，唔係介面設計。佢哋唔會被標記，必須喺任何重組時保留。

顏色／token／排版層次已經基本完成（睇吓 [`md3-design-system.md`](md3-design-system.md)）。呢個登記冊追蹤嘅殘留不符規範嘅地方主要係三個野：

1. **舊有光柵／SVG 圖示**，套件指定 Material Symbols 字形（`MaterialIcon` 幫助函數存在，但只喺少數檔案中採用）；
2. **舊有組件／對話框解剖學同幾何**：正方形圓角、原生 OS 視窗邊框、固定像素尺寸、手製控制項形狀；同埋
3. **少數未遷移嘅面板同缺失嘅共用組件**（冇共用 SearchField／Slider／SegmentedControl，冇繪製嘅 Checkbox/Radio，冇套件對話框殼）。

## 點樣使用呢個登記冊

- 每個**介面**部份下面都有一個間隙表。欄位，按順序係：**ID · 舊有元素 · 套件參考 · 原生錨點 · 必要改動 · 尺寸 · 風險 · 狀態**。
- 每個間隙都係**狀態 = 開啟**。只有喺改動已實施並喺應用程式中驗證過、喺**淡色 + 深色**、同埋：喺相關情況下，**預覽（紫色）**同**設備（青綠色）**方案中驗證過時，先可以閂咗一行。
- **ID** 係穩定嘅標籤；喺 commits/PRs 中引用佢。
- **尺寸**：細／中／大（實施工作量）。**風險**：細／中／高（回歸影響範圍）。
- **原生錨點**路徑相對於版本庫（`src/slic3r/GUI/...`）；行號由每個審計員根據當前分支提示重新驗證。
- **去重複化。** 若然兩個介面命名相同嘅原生錨點同相同嘅改動，間隙會作為一行保留喺**最特定嘅**介面，並由另一個移除。倖存行同被移除行都會帶有**合併備註**。應用咗四個咁嘅合併（睇吓受影響表下面嘅備註）；登記冊保留**128** 個唯一開啟間隙，從 **132** 個原始審計發現合併。
- **分波計劃**將開啟間隙排序成可並行化嘅實施波次，每個並行羣組有不相交嘅檔案所有權。**覆蓋範圍**部份逐字引用每個審計員嘅覆蓋範圍備註，所以未來會談知道確切咩被審計：同埋咩冇。

---

## 第二次掃過：通用元素同詞彙（2026-09-02）

登記冊上面閂咗*設計過*嘅解剖學。第二次機械掃過測量咗設計資料夾喺出貨嘅程式碼入面仲有咩未有對應：庫存 wxWidgets 控制項，仍然喺原生 OS 邊框上嘅對話框，同埋產品詞彙。規則：冇嘢會被移除，每個庫存元素會變成佢嘅套件組件，術語唔會改，除咗喺每個用戶睇到嘅字串入面，**filament 一律叫「墨水」，AMS 一律叫「墨水機」**。由 `ui-md3/tests/md3-conversion-contracts.test.mjs` 固定（「第二次掃過」測試）。

| ID | 舊有元素 | 套件參考 | 原生錨點 | 改動 | 狀態 |
|---|---|---|---|---|---|
| vocabulary-ink-dispenser | 每個 `_L()`/`_u8L()`/`L_str()`/libslic3r 回調字串仲係寫住 Filament／AMS | readme.md「產品」；TabBar.jsx `Ink`；Device.jsx `Ink Dispenser` | `GUI/LanguageMode.cpp` `vocabulary()`、`GUI/I18N.hpp`、`GUI/GUI_App.cpp:927` | 喺轉換邊界有一次重寫：全字、大小寫形式保留（Filament/filament/FILAMENT(+s) → Ink/ink/INK(+s)、AMS → Ink Dispenser）；識別符（`_` 冇空格）同 URL（`://`）永遠唔會變；目錄、設定鍵同源字串保留佢哋嘅字 | 完成 |
| generic-buttons | 36 處 `new wxButton(` | actions/Button.jsx（藥丸、已填充/輪廓/文字/危險、h36/42/44） | DesktopIntegrationDialog、GUI_AuxiliaryList、SysInfoDialog、SelectMachine 延時攝影對話框、SendSystemInfoDialog（3 個對話框）、BBLStatusBar／ProgressStatusBar 取消、Jobs/SLAImportJob | `Button` + `SetVariant()`；其餘四個檔案儲存點陣圖持有人或只有開發人員瀏覽器工具列（喺合約測試中允許列表） | 完成 |
| generic-gauges | 7 處 `new wxGauge(` | containment/ProgressBar.jsx（8px 軌道、r6、主要填充） | BBLStatusBar{,Send,Print,Bind}、ProgressStatusBar、Plater 刷新彈出式、Overview/AssemblyExportProgressWindow | 套件 `ProgressBar`，得到量表介面（`GetValue/GetRange/SetRange`）同不定式 `Pulse()` 掃過 | 完成 |
| generic-static-lines | 18 處 `new wxStaticLine(` | 1px OutlineVariant 分隔線 | UpgradePanel（11）、BBLStatusBar（2）、AMSDryControl（2）、AboutDialog | 套件 `StaticLine`（`SetLineColour` 而唔係視窗背景） | 完成 |
| generic-hyperlinks | 20 處 `new wxHyperlinkCtrl(` | Link token、底線、鍵盤/AT 連結角色 | SendToPrinter、BindDialog、SelectMachine（2）、ReleaseNote（2）、AMSMaterialsSetting、Widgets/SideTools（2）、CreatePresetsDialog（2）、MsgDialog（3）、SelectMachinePop、DownloadProgressDialog（2）、WebUserLoginDialog、Plater（3）、DeviceTab/wgtMsgPanel | 套件 `LinkLabel`；自訂點擊處理程序移至 `EVT_LINK_LABEL_LEFT_DOWN`，字型至內部標籤，顏色至 `SeLinkLabelFColour` | 完成 |
| generic-checkboxes | 10 處 `new wxCheckBox(` | selection/Checkbox.jsx（20px 字形 + 標籤） | UnsavedChangesDialog、ConfigWizard（3）、StepMeshDialog（2）、Tab（2）、TextureImportDialog、GUI_Utils 檔案對話框額外、AMSDryControl | 新套件列 `Widgets/LabeledCheckBox`（CheckBox 字形 + Label、重新發送 `wxEVT_CHECKBOX`）用於有標籤嘅位置；無標籤切換器用裸 `CheckBox` | 完成 |
| generic-choices-combos | 3 個 `new wxChoice(` + 3 處仍在使用嘅 `new wxComboBox(` | fields/SelectField.jsx | ConfigWizard gcode 選擇器、Widgets/MultiNozzleSync（2）、Jobs/SLAImportJob（2） | 唯讀套件 `ComboBox`（`wxEVT_CHOICE` → `wxEVT_COMBOBOX`） | 完成 |
| generic-sliders | 2 處 `new wxSlider(` | selection/Slider.jsx | Field.cpp SliderCtrl、SmartHomeDialog 音量 | 套件 `Slider` 配 `SetOnChange`；SmartHome 軌道條因無障礙而被擱置，`SliderAccessible` 而家提供 | 完成 |
| generic-text-inputs | 7 個單行 `new wxTextCtrl(` 輸入 | fields/ValueField.jsx／TextInput | AMSDryControl（2）、CreatePresetsDialog（3）、PhysicalPrinterDialog、ConfigWizard 檔案名稱 | 套件 `TextInput`（編輯器通過 `GetTextCtrl()` 到達） | 完成 |
| legacy-dialog-chrome-residue | 6 個自有對話框仲然用 `wxDEFAULT_DIALOG_STYLE`／`wxCAPTION` 同冇 MD3 說明文字 | containment/Dialog.jsx（無邊界殼、44px 說明文字） | ParamsDialog、AuxiliaryDialog、GUI_ObjectTable ObjectTableDialog、DesktopIntegrationDialog、SendSystemInfoDialog（3）、Gizmos/GLGizmoSlaSupports 幫助 | `MD3DialogCaption::Adopt()` 作為每個 ctor 嘅最後佈局動作 | 完成 |
| generic-radios | 7 處 `new wxRadioButton(` | selection（RadioBox 字形） | Widgets/AMSItem FeedDirectionDialog（3）、CalibrationWizardPresetPage（3，一個交給 `SetRadioBox(wxRadioButton*)`）、SavePresetDialog（3 喺一個羣組中） | 套件 `LabeledRadioButton`（RadioBox 字形 + Label、可聚焦、wxAccessible 無線電角色、空格/Enter）喺 `RadioGroup` 內部（排他性、上／下／左／右／主頁／結尾）；`FilamentComboBox::SetRadioBox` 接收套件列；FeedDirectionDialog 用 `GetSelection() == -1` 而唔係隱藏助手無線電（2026-09-05） | 完成 |
| generic-text-views | 多行／唯讀 `wxTextCtrl` 視圖同編輯器 | 冇套件文字區組件 | UnsavedChangesDialog diff、UpdateDialogs 更改日誌、MsgDialog 指令碼、WebViewDialog 頁面/來源、SendSystemInfoDialog JSON、NetworkTestDialog 日誌、StatusPanel 評論、MixedFilamentDialog 比例編輯器、ExtraRenderers 儲存格編輯器、Field.cpp 滑塊讀出、Overview/AssemblyPdfExportDialog（幫助 API 接收 `wxTextCtrl*`）、WebViewDialog 開發 URL 欄 | 套件 `TextArea`（輪廓容器、主要焦點環、radius_tiny、SurfaceContainerLowest／-Low 唯讀、等寬選項）通過 `GetTextCtrl()` 裝載原生編輯器；喺 UpdateDialogs、MsgDialog、SendSystemInfoDialog、NetworkTestDialog、StatusPanel 評論、兩個 WebViewDialog 檢視者、UnsavedChangesDialog 儲存格中採用；單行迷失兒移至 `TextInput`。因錄製原因保留原生：套件內部編輯器、正則表達式製造者欄位、dataview 儲存格編輯器、混合墨水比例儲存格編輯器、開發人員專用 URL 欄（2026-09-05） | 完成 |
| generic-listbox | 1 個 `new wxListBox(` | 套件中無 | SmartHomeDialog 實體列表 | 套件 `ListBox`（所有人繪製 wxVListBox、DropDown 列解剖學、省略符號 + 工具提示）喺 SmartHomeDialog（2026-09-05） | 完成 |
| fatal-path-message-boxes | 5 個現場 `wxMessageBox(` 呼叫 | containment/Dialog.jsx | GUI_App.cpp（Fatal error、Critical error、第一次載入語言）、GUI_Init.cpp（初始化失敗，兩個） | 喺 GUI 被清除前或當時發火，其中 MD3 殼可能無法建構；刻意原生 | 偏離 |
| stock-input-dialogs | 11 個內置提示、揀選器同忙碌通知：`wxTextEntryDialog`（5）、`wxNumberEntryDialog` 同 `wxGetNumberFromUser`（2）、`wxMultiChoiceDialog` 同 `wxGetSelectedChoices`（2）、`wxBusyInfo`（2），加埋其他平台嘅 `wxGetSingleChoiceIndex` 分支 | containment/Dialog.jsx; fields/ValueField.jsx; selection/Checkbox.jsx | AppearanceEditorPopover（「另存為預設」）、WorkspacePanel（提示）、Plater（「克隆數量：」、「克隆」、「重新載入來自：」、「替換來自：」）、GUI_Factories（層範圍設定）、Tab（相容預設）、WebViewDialog（開發者腳本提示）、GUI_App（單選） | `TextEntryDialog`、`NumberEntryDialog` 同 `MultiChoiceDialog` 建基於 MsgDialog 外殼，用套件 `TextInput`／`TextArea`、`SpinInput` 同 `LabeledCheckBox`；`BusyInfo` 係喺阻住事件迴圈嘅工作開始之前已經畫好嘅圓角 SurfaceContainerHigh 面板；每個平台都用 `SingleChoiceDialog`；`stock-dialogs.test.mjs` 會拒絕內置版本（2026-09-29） | 完成 |
| stock-colour-dialogs | 3 處 `wxColourDialog`：Windows 顏色對話框，連同佢十六格自訂顏色，同用系統外觀同語言嘅掣 | Material 揀色器（`MD3ColorPickerDialog`） | AMSMaterialsSetting（墨水機槽位顏色）、PresetComboBoxes（墨水顏色）、wxExtensions `show_sys_picker_dialog`（墨水揀選器嘅「更多顏色」、紋理匯入、預設選單本身嘅揀色器） | `pick_filament_color()`：Material 揀色器，不透明，將系統對話框保存嘅最近用過嘅顏色做「最近使用」快速選項；確定咗嘅顏色會加入去；批量墨水對話框同設定頁嘅顏色欄用同一份清單（2026-09-29） | 完成 |
| native-tip-and-info-bar | 停用咗嘅套件 Button 嘅提示用 `wxTipWindow`，網頁雲端通知用 `wxInfoBar`：系統淡色提示方塊，同系統資訊顏色、圖示同原生掣 | 純文字工具提示（InverseSurface）；橫額 | Widgets/Button.cpp、WebViewDialog.cpp | `ButtonDisabledTip`：放喺永遠唔會攞走指標或者焦點嘅彈出視窗入面嘅 Material 純文字工具提示；`MD3InfoBanner`：有狀態圖示、套件文字動作同關閉掣嘅 SurfaceContainerHigh 長條（2026-09-29） | 完成 |
| workspace-native-controls | 工作區面板嘅 `wxNotebook`、兩個 `wxListCtrl` 報告、`wxCheckListBox` 同 `wxCalendarCtrl`：Windows 分頁控件、清單檢視、待辦清單同月曆 | navigation/TabBar.jsx; selection/Checkbox.jsx | WorkspacePanel.cpp | 喺 `wxSimplebook` 上面嘅 `TextTabbar`（而家用 Material 角色）；用 `md3_style_data_view()` 嘅 `wxDataViewListCtrl` 表格；加咗 `EnableChecks()` 嘅套件 `ListBox`；Material 顏色、逐月切換嘅 `wxGenericCalendarCtrl`；`native-controls.test.mjs` 會拒絕原生類別（2026-09-29） | 完成 |
| developer-log-window | `MainFrame::show_log_window()` 打開嘅 `wxLogWindow` | 無 | MainFrame，由「偏好設定」、「開發者工具」、「內部開發者模式」打開 | 「開發者工具」分頁唔會編譯入任何發佈版本（`BBL_RELEASE_TO_PUBLIC=1`），所以冇一個發佈版本可以打開佢；刻意保留原生 | 偏離 |
| static-bitmaps | 175 處 `new wxStaticBitmap(` | 套件顯示圖示嘅地方係 Material Symbols 字形；產品相片／圖表係資料 | 喺整個 GUI 樹中 | 手工審查清單 `static-bitmap-triage.csv`（清掃後 146 處仍在使用）：12 個可點擊圖片控制項變成套件圖示按鈕、48 個圖片控制項用 MD3 角色中嘅 Material 字形、98 個係內容影像或呼叫方提供嘅資料且記錄咗原因若非明顯、8 個未填充 MonitorBasePanel 圖片控制項刪除、兩個度數標記係排版標籤。檢查只接受 `data` 同 `md3-rendered` 兩種判定，並固定每個已轉換嘅位置。執行時捕獲等待本地構建（2026-09-05） | 完成（來源） |

## 第三次掃過：每一個元素（2026-09-05）

用戶重新陳述嘅授權：**每一個渲染元素都係 Material Design 3** 同每一個佈局裁剪缺陷都修復咗，配實時構建物捕獲。由 2026-09-05 測試喺 `ui-md3/tests/md3-conversion-contracts.test.mjs` 中固定；每個防衛都喺刻意回歸前被證實為紅色之前被信任。

| ID | 舊有元素 | 套件參考 | 改動 | 狀態 |
|---|---|---|---|---|
| generic-static-text | 548 處仍在使用嘅 `new wxStaticText(` | Label（MD3 body 類型、OnSurface） | 由 `scripts/md3/convert-static-text.mjs` 進行指令碼重新輸入；Label 種子 OnSurface／主要連結音調及其 `SetLabel` 比較原生標籤，所以基本 `Wrap()` 無法留下陳舊文字 | 完成 |
| generic-bitmap-buttons | 11 處 `new wxBitmapButton(` | actions/IconButton | 套件圖示 `Button`／`RadioBox`；`ScalableBitmap` 包裝現成點陣圖同 `Button::SetIconBitmap` 裝載資料樣本；選擇器選擇環係套件邊框 | 完成 |
| button-md3-default | `Button` 舊有白色/綠色預設外觀 | actions/Button | 輪廓變體喺首次繪製時除非變體或呼叫者風格被設定；Outlined/Text 定義 Checked；Button.cpp 中冇 `ThemeColor` | 完成 |
| density-live-reads | 15 個固定 `Metrics::comfortable/compact` 讀取 | density.css | `Metrics::active()` 在 StaticBox 種子同其密度切換器之外的任何地方 | 完成 |
| image-switch-button-palette | `ImageSwitchButton`／`FanSwitchButton` 舊有調色盤 | selection/Switch | Surface／Primary／SurfaceContainerHighest／OnSurface 角色 | 完成 |
| ams-control-macros | `AMS_CONTROL_*` 宏 | tokens | 已審查並保留：token 淡色別名深色對應喺繪製時；預解析會雙重對應 | 偏離 |
| imgui-chrome-literals | 重疊板、工具提示文字、懸停列文字、禁用複選框灰色、字幕規則 | tokens | `md3_imgui_color`／`md3_imvec4` 角色；透明填充、影像著色同資料顏色不變 | 完成 |
| layout-probe | 冇機械裁剪偵測 | -- | `LayoutProbe.{hpp,cpp}`（BAMBU_LAYOUT_PROBE、WM_COPYDATA dwData 2）、`ui-md3/tests/layout-probe-report.mjs` | 完成（執行時掃過等待構建嘅二進制） |
| unscaled-sizes | 21 個文字 `Set*Size(wxSize(N, M))` 像素尺寸，一個標籤配三種省略號風格 | -- | `FromDIP` 到處都係；`wxST_ELLIPSIZE_END` 只有一個；防衛禁止兩者 | 完成 |
| button-legacy-palettes | 201 次手風格 `SetBackgroundColor/SetBorderColor/SetTextColor` 喺 47 個檔案中套件 Button 上執行 | actions/Button | `scripts/md3/convert-button-styling.mjs` 重寫每次執行至 `SetVariant(Filled)` 或 `SetVariant(Outlined)` 從正常態發光度；取消家族名稱去 Outlined；已從 MD3 角色幫助者構建嘅調色盤被保留；六個剩餘嘅手轉換 | 完成（來源） |
| helio-partner-branding | 9 次喺 Helio 對話框同 Helio 卸載按鈕上執行使用夥伴紫色／藍色調色盤（`theme.purple`、`theme.blue`、`HELIO_*`） | -- | 保留：呢啲按鈕喺夥伴提供嘅流程內攜帶第三方品牌顏色；記錄喺呢度並被代碼 mod 嘅縮減棘輪保留（9） | 偏離 |
| device-placeholder-page | `web/device/missing_connection.html`：Arial、硬編碼灰色、100vh flex 中心化裁剪佢嘅標題、四個死指令碼連結 | tokens／卡片 | Surface 同容器角色、headline-small 同 body-medium、大形狀、level-1 高度；body 係滾動容器，只喺卡片適合時中心化；深色檔案只重新定義 tokens；OnInit 呼叫佢加載嘅轉換表 | 完成（來源；捕獲待定） |
| project-empty-state-font | `web/model_new/css/black.css` 冇 font-family，所以項目空態同其按鈕喺瀏覽器嘅 serif 中呈現 | typography | 套件面 token 喺 body 同按鈕上、body-medium 同 label-large、冇預設 body 邊距、安全中心化 | 完成（來源；捕獲待定） |

驗證呢個掃過：每個編輯翻譯單位用 `g++ -fsyntax-only` 根據 Linux 上嘅 wxWidgets 3.2／Boost／OpenCASCADE 標題編譯（Windows 專用邊框路徑喺 `#ifdef _WIN32` 內部）。Windows 構建由成功執行驗證 [`33695532981`](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/33695532981) 同 [`33696115604`](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/33696115604)，包括應用程式構建、未簽名 Squirrel 套件驗證同不可變發佈發行。每個轉換介面嘅淡色同深色捕獲仍然開啟。問題 #28 只係編譯器而冇可見狀態，所以佢嘅構建證據唔代替呢啲捕獲。

---

## 介面：widgets-core（共用 Widgets 庫，第 1 部份）

| ID | 舊有元素 | 套件參考 | 原生錨點 | 必要改動 | 尺寸 | 風險 | 狀態 |
|----|----------------|---------|---------------|-----------------|------|------|--------|
| checkbox-drawn-glyph | `wxBitmapToggleButton` 交換 9 個烤製 18px PNG（check_on/half/off × normal/disabled/focused）；顏色喺資產中凍結、冇深色/方案重新著色、18px 唔係 20px | selection/Checkbox.prompt.md：20px check_box/check_box_outline_blank 字形、已檢查主要／未檢查 OnSurfaceVariant、現場繪製、標籤 12.5/500 | Widgets/CheckBox.cpp:6 | 已繪製複選框：呈現 check_box／check_box_outline_blank 作為 Material Symbols（將碼位加到 MaterialIcon，或繪製 20px 正方形 + Check）、已檢查=Primary[,scheme]／未檢查=OnSurfaceVariant 現場；通過第三字形保留三態；放棄 9 個 ScalableBitmap | 大 | 中 | 完成 |
| radiobox-drawn-glyph | `wxBitmapToggleButton` 配烤製 radio_on/off/ban 18px PNG；顏色凍結、冇主題/方案重新著色；禁用時用 ban 字形 | selection/（冇 Radio 組件；radio_button_checked/unchecked 已喺 MaterialIcon.hpp:47-48） | Widgets/RadioBox.cpp:9 | 通過 MaterialIcon RadioButtonChecked/Unchecked、選定=Primary[,scheme]／未選定=OnSurfaceVariant、禁用時變淡 OnSurfaceVariant（放棄 ban 資產）現場繪製無線電 | 中 | 細 | 完成 |
| switchbutton-md3-switch | 圖示切換模式用烤製 toggle_on/off 16px PNG；標籤模式手繪完整藥丸、軌道=Grey350 原始、拇指=BrandGreen(on)/Grey350；冇 2px 環、冇透明關軌道、冇生長拇指；文字 White/TextMuted | selection/Switch.prompt.md：軌道 44x24 r14、2px 邊框已檢查主要/未檢查大綱、填充主要/透明、拇指 12->16px OnPrimary/Outline .15s | Widgets/SwitchButton.cpp:24 | 將 Switch 繪製到規格：44x24 軌道、2px 邊框（主要／大綱）、填充主要開/透明關、拇指滑動 4->22 並在 150ms 上生長 12->16px；移除切換 PNG 同 Grey350 文字 | 大 | 中 | 完成 |
| switchboard-segmented-tokens | SwitchBoard 手繪 2 段切換：bg 白色、軌道 Grey300/Grey400、活躍 BrandGreen（唔係方案感知）、文字 White/TextPrimary、半徑 8、冇間隙、冇 sc-highest 軌道 | selection/SegmentedControl.prompt.md：容器 sc-highest r12/14 pad3/4 gap4/6、選定 Primary/OnPrimary r9/11、未選定透明/OnSurfaceVariant | Widgets/SwitchButton.cpp:275 | 重新裝飾為 2 選項 SegmentedControl：容器 SurfaceContainerHighest、活躍主要[,scheme]/OnPrimary、非活躍透明/OnSurfaceVariant、內環半徑 + 段間間隙每套件、方案通過；移除 White/Grey300/Grey400/BrandGreen | 中 | 中 | 完成 |
| textinput-md3-field-geometry | 半徑 0 正方形；bg 白色/Grey300(禁用)；邊框 Grey400 靜態 + BrandGreen 懸停；前導圖示 16px 光柵；單位/前綴喺 TextDisabled | fields/SelectField + ValueField.prompt.md：已填充 r10 bg sc-highest；輪廓 r10 1px 大綱；單位 OnSurfaceVariant | Widgets/TextInput.cpp:30 | 半徑 10；SurfaceContainerHighest 填充（或透明 + 1px 大綱輪廓）；靜態邊框大綱（唔係 OutlineVariant）；前導圖示作為 MaterialIcon 字形；單位/前綴 OnSurfaceVariant；重置有效恢復欄位 bg 角色，唔係白色（TextInput.cpp:443） | 大 | 中 | 完成 |
| combobox-selectfield | 繼承 TextInput 半徑-0 正方形；尾隨優惠係光柵「drop_down」PNG；唯讀 Focused bg 原始 0xEDFAF2（唔係 SecondaryContainer #d7e8d9）喺 ComboBox.cpp:69 | fields/SelectField.prompt.md：已填充 h34 r10 sc-highest expand_more 16；輪廓 h38 r10 1px 大綱 expand_more 18；OnSurfaceVariant 雪佛龍 | Widgets/ComboBox.cpp:48 | 採用 SelectField 幾何（r10、sc-highest 已填充或 1px 大綱輪廓）；尾隨雪佛龍 = MaterialIcon::ExpandMore 16/18px OnSurfaceVariant；替換 0xEDFAF2 焦點填充為 SecondaryContainer[,scheme] | 中 | 中 | 完成 |
| dropdown-floating-surface | 彈出半徑 0 硬矩形、冇陰影；選定列原始 0xEDFAF2；懸停/選定列硬 1px 矩形；檢查/箭頭光柵 PNG；滾動條 Grey300/Grey400 | native-dialogs-widgets 摘要：SurfaceContainer 表面、OutlineVariant 邊框、r~18、高度、SecondaryContainer 選定／SurfaceContainerHigh 懸停 | Widgets/DropDown.cpp:50 | 圓形浮動表面：r~18、SurfaceContainer bg、1px OutlineVariant、放棄陰影；懸停 SurfaceContainerHigh + 選定 SecondaryContainer[,scheme] 圓形填充（替換 0xEDFAF2）；檢查/箭頭 → MaterialIcon Check/ChevronRight；滾動拇指 OutlineVariant | 大 | 中 | 完成 |
| spininput-valuefield | 半徑 0 正方形、bg 白色/Grey300、邊框 Grey400/BrandGreen 懸停；步進器係帶光柵 spin_inc/dec 6px PNG + Grey400 分隔線嘅按鈕；值用 Body_14（Roboto 比例）唔係 mono | fields/ValueField.prompt.md：h34 r10 bg sc-highest、值 Roboto Mono 12.5/500、單位 OnSurfaceVariant | Widgets/SpinInput.cpp:31 | ValueField 幾何：r10、SurfaceContainerHighest 填充；值喺 Roboto Mono 12.5/500；步進器 → MaterialIcon 字形圖示按鈕（Add/Remove 或 ExpandLess/More）；放棄 Grey400 分隔線 | 中 | 中 | 完成 |
| button-iconbutton-and-glyph-icons | Button enum 已填充/Tonal/輪廓/文字/危險冇 IconButton；圖示永遠光柵 ScalableBitmap 即使喺 MD3 模式；預設 ctor 為每個冇指定變體嘅呼叫位置保持舊有白色/Grey200/BrandGreen + 半徑 20 | actions/IconButton.prompt.md（無邊框 ghost 樣式、圓圈 50% ／正方形 r8、懸停 sc-high、危險懸停錯誤/OnError、字形 17-22 OnSurfaceVariant） + Button.prompt.md | Widgets/Button.hpp:15 | 增加 IconButton 模式（無邊界；圓圈 50% 或正方形 r8；靜態透明 OnSurfaceVariant；懸停 SurfaceContainerHigh；選項已填充 sc-highest；危險懸停錯誤/OnError）；通過 MaterialIcon 字形而唔係 ScalableBitmap 路由按鈕/IconButton 圖示 | 大 | 中 | 完成 |
| customtogglebutton-chip-anatomy | 顏色係語義（好）但仍然硬編碼 DrawRoundedRectangle 半徑 5、光柵 switch_send_mode_tag_on/off 16px PNG 圖示、未選定邊框 Grey300 | selection/Chip.prompt.md：藥丸 r=h/2、1px 邊框、16px 字形、選定主要/OnPrimary 或 SecondaryContainer 色調 | Widgets/SwitchButton.cpp:447 | 重新塑造向 Chip：藥丸半徑（高度/2）、1px 大綱/主要邊框、MaterialIcon 字形替換 switch_send_mode_tag PNG；保留已正確語義主要/SecondaryContainer 填充 | 細 | 細 | 完成 |
| no-shared-md3-searchfield | 冇共用 SearchField 組件存在；搜尋 UI 係臨時：原始 wxSearchCtrl（Plater.cpp:3171；SelectMachinePop.cpp:338）或 StaticBox+TextInput 半徑 5（Tab.cpp:330）；無配 40px sc-highest r22 藥丸 | fields/SearchField.prompt.md：40px sc-highest 藥丸 r22、邊框大綱->主要焦點、搜尋字形 20 OnSurfaceVariant、.* regex + 調整切換、30px 清晰圓圈 | Plater.cpp:3171 | 介紹共用 SearchField 組件到規格並替換原始 wxSearchCtrl 實例同 Tab.cpp 半徑-5 組件 | 大 | 中 | 完成 |
| no-shared-md3-slider | 冇共用 MD3 Slider 組件；原始 wxSlider（Field.cpp:2106；StepMeshDialog.cpp:143,196）配 OS 外觀 + 舊有自訂 GreenSlider（TextureImportDialog.cpp:180）；無著色為主要或用 OutlineVariant 非活躍軌道 | selection/Slider.prompt.md：~4px 軌道、主要活躍、OutlineVariant 非活躍、主要圓形拇指；垂直 12x300 | Field.cpp:2106 | 提供共用 MD3 Slider（瘦軌道、活躍主要[,scheme]、非活躍 OutlineVariant、主要圓形拇指；垂直 12px 變體）同喺原始 wxSlider／GreenSlider 位置採用佢 | 中 | 細 | 完成 |

---

## 介面：widgets-containment（共用 Widgets 庫，第 2 部份，對話框、卡片/面板、進度、快餐欄、段標題、滾動條、標籤欄、工具提示）

鑑於文件嘅規模，讓我分節完成翻譯。我會繼續翻譯其餘部份。

| ID | 舊有元素 | 套件參考 | 原生錨點 | 必要改動 | 尺寸 | 風險 | 狀態 |
|----|----------------|---------|---------------|-----------------|------|------|--------|
| msgdialog-base-shell-anatomy | MsgDialog 基礎係原生 OS 邊框視窗（DPIDialog、wxDEFAULT_DIALOG_STYLE）配圖示-LEFT（64px 光柵標誌）+ 內容-右 + 按鈕列；bg 係 SurfaceContainer 但冇套件殼（冇 28px 無邊界框、冇 elev-5、冇標題圖示瓷磚 + 標題/字幕、冇圓形關閉、冇頁尾邊框） | containment/Dialog.jsx + Dialog.prompt.md；shape-elevation.html（28px 對話框半徑、elev-5） | MsgDialog.cpp:29 | 重建基礎作為套件對話框殼：無邊界 28px 框、elev-5 陰影、標題 = 44x44 PrimaryContainer 圖示瓷磚（r14）+ Material Symbol 字形 + 標題 18/600 + 字幕 12.5 OnSurfaceVariant + 36px 圓形關閉；body pad 0/24 gap 16；頁尾 pad 14/24 flex-end 配 1px OutlineVariant 頂邊框；替換左光柵標誌為標題圖示瓷磚（瀑布到 ~12 子類） | 大 | 高 | 完成 |
| msgdialog-footer-button-geometry | 頁尾按鈕用 SetCornerRadius(12)（唔係藥丸）、固定 58/76/90 x 24px、Body 字型、由「第一焦點 = 綠色已填充、其餘輪廓」啟發式風格化；h24 遠低於套件 sm/md/lg | containment/Dialog.prompt.md 頁尾；actions/Button.jsx（藥丸 r=h/2、尺寸 36/42/44） | MsgDialog.cpp:139 | 用套件 Button 變體風格化頁尾按鈕（文字用於取消/輔助、已填充用於主要動作）、藥丸半徑（高度/2）同套件高度（36/42/44） | 中 | 中 | 完成 |
| dpidialog-subclass-topline-chrome | DeleteConfirm/Newer3mf/NetworkError DPIDialog 對話框分享舊有 Bambu 解剖學：OS 標題欄（wxCAPTION+wxCLOSE_BOX）+ 1px OutlineVariant `m_line_top` 頂重點分隔線 + 半徑-12 h24 Body_12 頁尾按鈕。顏色遷移；結構唔係套件對話框 | containment/Dialog.jsx（標題 + 關閉、冇頂分隔線）；shape-elevation.html（28px 半徑） | MsgDialog.cpp:565 | 重新父級到套件對話框殼（28px 無邊界、標題圖示瓷磚 + 標題/字幕 + 圓形關閉、elev-5）；刪除 `m_line_top` 分隔線；轉換頁尾按鈕為套件變體 + 藥丸半徑 + 套件高度 | 大 | 高 | 完成 |
| progressdialog-shell-white-bg | 切片 ProgressDialog 係原生 OS 邊框；bg 係原始 ThemeColor::White 直接傳至 SetBackgroundColour（繞過深色對應、喺深色中保持白色）喺對話框 + 內部面板上、加上 Grey450 頂線同原生 wxGauge | containment/Dialog.jsx；colors-surfaces.html（對話框 bg SurfaceContainer）；shape-elevation.html（28px 半徑） | Widgets/ProgressDialog.hpp:20 | 將 PROGRESSDIALOG_DEF_BK 白色替換為 SurfaceContainer；放棄 Grey450 頂線；採用套件對話框殼；通過遷移 ProgressBar（主要）而唔係原始 wxGauge 呈現進度 | 中 | 中 | 完成 |
| notification-snackbar-inverse-roles-placement | Canvas toast 解析 MD3 角色但錯誤：bg SurfaceContainerHigh + 文字 OnSurface（淺光淺卡片）、唔係套件嘅 InverseSurface/InverseOn/InversePrimary 深 toast；半徑 4（套件 12）；1px 邊框、冇陰影（套件 0 8px 24px）；右/底錨定（套件底中心欄） | containment/Snackbar.jsx + Snackbar.prompt.md（InverseSurface/InverseOn/InversePrimary、r12、陰影、底中心 min(560px,92vw)） | NotificationManager.cpp:147 | 顏色/半徑/陰影遷移到套件 toast（Inverse 角色、r12、elev-4）。放置係記錄刻意偏離：堆棧錨定到底右角（16px 邊距）、唔係套件嘅底中心欄，非阻擋通知係必需（產品授權）堆棧喺螢幕角落所以佢哋永遠唔會覆蓋板中心注意力 | 大 | 高 | 偏離 |
| progressbar-geometry-pill-height | 顏色遷移（sc-highest 軌道／主要填充）但幾何舊有：預設高度 miniHeight 14（套件 8）、半徑高度/2（完全藥丸；套件 r6）、佢烤製中心「%NN%」標籤；禁用填充用原始警告（橙色） | containment/ProgressBar.jsx（高度 8、軌道 sc-highest r6、填充主要 r6、冇烤製文字） | Widgets/ProgressBar.hpp:30 | 預設高度 8、半徑 6（柔和圓形、唔係高度/2 藥丸）；放棄/外部化烤製百分比文字；通過語義角色解析任何禁用/受阻視覺、唔係原始警告 | 細 | 細 | 完成 |
| tabctrl-secondary-tab-indicator-text | 輔助子標籤欄（校準）用原始 BrandGreen（唔會重新著色為預覽/設備）繪製其活躍指示器喺半徑 1；標籤 TextMuted（未檢查）+ 原始 wxLIGHT_GREY（正常）；選擇僅由加粗進行；冇圖示 | navigation/TabBar.jsx + TabBar.prompt.md（活躍主要/600 + 填充-1 圖示、非活躍 OnSurfaceVariant/400、3px 圓形主要指示器插圖 12） | Widgets/TabCtrl.cpp:313 | 指示器 → 主要(當前方案)配圓形 3px 幾何（插圖 12）；活躍文字主要/600、非活躍 OnSurfaceVariant/400；增加 Material Symbol 圖示配活躍標籤上填充-1 | 中 | 細 | 完成 |
| tabbook-selected-tab-anatomy | Tabbook（設定/打印機）將選定標籤呈現為已填充 SecondaryContainer 正方形藥丸（SetCornerRadius 0）+ 底部標記 + 底部標籤填充喺 TextPrimary 中繪製（中立、唔係主要）；標籤用 PNG 圖示（monitor_arrow、monitor_hms_new） | navigation/TabBar.jsx + TabBar.prompt.md（透明標籤、主要文字、3px 圓形主要指示器） | Tabbook.cpp:65 | 採用套件選擇模式：透明標籤、主要/600 活躍標籤、3px 圓形主要指示器（方案感知）而唔係已填充正方形 SecondaryContainer 標籤 + 中立標記；遷移箭頭/新標籤圖示為 Material Symbols | 中 | 中 | 完成 |
| scrollbar-chrome-white-square | 自訂滾動條繪製純填充矩形（冇半徑）用於拇指/提示/邊距；拇指預設原始白色；擦除繪製白色（非深色自適應） | base.css 滾動條（10px、圓形 OutlineVariant 拇指 r8、2px 插圖、懸停大綱、透明軌道） | Widgets/Scrollbar.cpp:32 | 繪製圓形拇指（r~8）喺 OutlineVariant、懸停大綱；透明軌道；移除原始白色拇指/擦除填充 | 細 | 細 | 完成 |
| staticbox-card-no-hover-border | 基本卡片原始預設半徑為緊湊半徑（12）無條件地（永遠唔會根據舒適 16 縮放）同冇互動懸停邊框提升（冇 OutlineVariant->主要懸停） | containment/Card.jsx + Card.prompt.md（bg sc-low、OutlineVariant->主要懸停互動、r16/12 通過密度、.15s） | Widgets/StaticBox.hpp:61 | 增加互動卡片模式，喺懸停時提升靜態 OutlineVariant 邊框為主要(~.15s)同從活躍密度而唔係固定 12 驅動預設半徑（舒適 16／緊湊 12） | 中 | 中 | 完成 |
| staticgroup-card-white-bg | StaticGroup（標題羣組框卡片）通過 SetBackgroundColour（繞過深色對應、喺深色中保持白色）設定 bg 為原始 ThemeColor::White 而唔係容器角色；Grey400 邊框很好 | containment/Card.jsx（bg sc-low、邊框 OutlineVariant） | Widgets/StaticGroup.cpp:13 | 設定羣組 bg 為 SurfaceContainerLow（或適當容器角色）所以佢係主題自適應 | 細 | 細 | 完成 |
| sectionheader-style-missing | 冇原生 SectionHeader 組件或 Label 幫助；字型庫有 Head_*/Body_*/Mono_* 但冇 11px/600/uppercase/+.6px OnSurfaceVariant 段標籤風格，所以面板回退到特臨時粗標籤 | containment/SectionHeader.jsx + SectionHeader.prompt.md（11px 600 +.6px 大寫 OnSurfaceVariant、選項領先圖示 16、gap 6） | Widgets/Label.hpp:40 | 增加共用 SectionHeader 風格/幫助（11px、600、大寫、+.6px、OnSurfaceVariant、選項 16px 領先 Material Symbol），Label 風格或細小組件 | 中 | 細 | 完成 |
| staticline-divider-color | 共用分隔線/分隔線組件預設其線顏色為 Grey300（SurfaceContainerHigh #e8e7ee），細毛太淡，其中套件用 1px OutlineVariant（#c5c6d0） | shape-elevation.html + 套件分隔線 = 1px OutlineVariant | Widgets/StaticLine.cpp:20 | 預設 StaticLine 嘅 lineColor 為 OutlineVariant 以匹配套件分隔線 | 細 | 細 | 完成 |

---

## 表面：chrome-nav（題目欄、工作空間標籤欄、模型軌欄項目、設定導覽）

| ID | 舊版元素 | 工具包參考 | 原生錨點 | 需要嘅變更 | 大細 | 風險 | 狀態 |
|----|---------|----------|---------|-----------|------|------|--------|
| titlebar-brand-tile-png | 品牌身份係一個 22px 點陣圖 PNG 標誌，透過 create_scaled_bitmap("BambuStudio") 新增為 ID_LOGO | navigation/TitleBar.jsx:12-15 (spec-navigation-shell.md §3.1) | BBLTopbar.cpp:246 | 將 PNG 換成工具包品牌磚：26x26 圓角矩形（r8）填充 Primary + on-primary 嘅 'deployed_code' 字形（18px，FILL 1），透過 MaterialIcon，elev-1 陰影，然後係 'Bambu Studio' 文字標記 500/14/OnSurface；更新 Init、sys-colour 重組（388-390）同埋 Rescale（626-628） | 中等 | 中等 | 完成 |
| titlebar-remove-nonkit-controls | 題目欄帶住儲存/復原/重做/校準/發佈動作控制元件，全部係舊版點陣圖 PNG（加埋 *_inactive 變體）以及一個額外嘅 AddSeparator；冇一個屬於工具包題目欄 | navigation/TitleBar.jsx（冇呢啲控制元件；native-chrome.md §3.2/§8.A.5） | BBLTopbar.cpp:277 | 移除標題欄到嘅儲存/復原/重做/校準/發佈（同埋 :266 分隔符 + 間隔符）；將佢哋嘅功能重新放置到 MD3 表面（準備底部動作欄 / 功能表）或者只保留作為明確批准嘅偏差 | 大 | 高 | 完成 |
| titlebar-missing-history-chip | 冇版本歷史/分支籌碼；git 項目歷史只能透過檔案功能表項目到達 | navigation/TitleBar.jsx:22-29 (spec-navigation-shell.md §3.5) | BBLTopbar.cpp:224 | 新增工具包歷史籌碼：h30、pad 0/12、r16、bg SurfaceContainer（懸停 High）、font 12；'account_tree' 字形 16 Primary + 分支名稱（Roboto Mono）+ 5x5 搏動 Primary 點 + '#'+head（mono）；連接到現有歷史後端 | 中等 | 中等 | 完成 |
| titlebar-project-chip | 項目名稱係一個置中嘅簡單 wxAuiToolBar 標籤（ID_TITLE，寬度 300），位於兩個延伸間隔符之間，而唔係工具包項目籌碼 | navigation/TitleBar.jsx:30-34 (spec-navigation-shell.md §3.6) | BBLTopbar.cpp:308 | 將置中嘅 ID_TITLE 標籤 + 雙延伸間隔符更換為單個右對齊拖動間隔符 + 工具包項目籌碼（'description' 字形 16 + 省略號名稱 max-w150、h30、pad 0/6、r20、bg SurfaceContainer、OnSurfaceVariant、font 12.5）；重用 update_responsive_title 省略號功能 | 中等 | 中等 | 完成 |
| controls-without-accessible-names | 針對真實標籤嘅重新拍攝通道顯示冇無障礙名稱嘅控制元件：散裝墨水、墨水列功能表、新增打印板、所有「一般」標籤開關同埋組合、兩個「瀏覽」按鈕、項目標籤關閉，以及側欄、導覽軌、項目標籤同埋頂欄區域；標題欄嘅工具冇幫助文字 | accessibility.md（每個可操作控制元件都命名本身） | Plater.cpp、MainFrame.cpp、Preferences.cpp、ProjectTabBar.cpp、Notebook.cpp、BBLTopbar.cpp | 喺每個控制元件同埋區域上設定名稱；喺視窗工具上設定 SetShortHelp；佈局探針發出工具記錄，所以名稱可以從轉儲入面驗證 | 中等 | 低 | 完成 |
| titlebar-appearance-palette-button | 冇「外觀/調色板」按鈕（同埋冇主題/密度/重點浮動視窗項目進入點） | navigation/TitleBar.jsx:35-38 (spec-navigation-shell.md §3.7) | BBLTopbar.cpp:305 | 喺視窗控制分隔符之前新增一個 34x34 圓形幽靈按鈕，帶住 'palette' 字形（20、OnSurfaceVariant、懸停 SurfaceContainerHigh）；將佢連接到 MD3 外觀浮動視窗（或者至少新增按鈕 + 存根觸發器） | 中等 | 中等 | 完成 |
| titlebar-window-controls-raster | 最小化/最大化/關閉使用舊版點陣圖 PNG（topbar_min/max/win/close）；幾何圖形唔係工具包 38x32 r8 方框 | navigation/TitleBar.jsx:40-46 (spec-navigation-shell.md §3.9) | BBLTopbar.cpp:331 | 將 min（'remove' 18）、max（'crop_square' 15）、close（'close' 18）渲染為 38x32 r8 按鈕裏面嘅 Material Symbols 字形；保留現有嘅關閉破壞性錯誤懸停同埋 1x22 分隔符；更新 Rescale 點陣圖網站（658-672） | 中等 | 中等 | 完成 |
| tabbar-icons-raster-no-fill | 工作空間標籤使用舊版自訂 SVG 點陣圖（tab_*_active.svg）作為 ScalableBitmap；相同嘅名稱同時用於活躍同埋非活躍，所以冇 FILL 0->1 輪廓/填充切換 | navigation/TabBar.jsx:2-12,25 (spec-navigation-shell.md §4) | MainFrame.cpp:1024 + Widgets/Button.cpp:388 | 將每個標籤對應到佢嘅工具包 Material Symbols 字形（home、view_in_ar、layers、cast、devices、folder_open、build、palette、settings），20px，透過 MaterialIcon 字型路徑，切換 FILL 0（非活躍）-> 1（活躍）；需要教授 Button/ButtonsListCtrl 繪製字型字形而唔係 ScalableBitmap | 大 | 中等 | 完成 |
| tabbar-hover-shape-rounded | 標籤懸停/選擇繪製為 48px 標籤儲存格內嘅 radius-8 圓形藥丸，而唔係工具包嘅平坦全高矩形 | navigation/TabBar.jsx:22-24 (native-chrome.md §4.3) | Notebook.cpp:153 | 將標籤懸停填充為全儲存格平坦（radius 0）邊對邊矩形，跨越標籤高度；將標籤按鈕角半徑設定為 0 或喺 ButtonsListCtrl::OnPaint 裏面繪製懸停層 | 細 | 低 | 完成 |
| tabbar-font-and-padding-metrics | 標籤標籤字型嚟自 normal_font()（em 驅動），唔係工具包 body-s 13.5px；每個標籤水平填充使用 compact.padding（10），唔係工具包嘅 16px | navigation/TabBar.jsx:22-26 (spec-navigation-shell.md §4) | Notebook.cpp:154 | 將標籤標籤字型釘住到 13.5px 同埋每個標籤水平填充釘住到 16px（圖示標籤間隔 8 已經正確）；保持活躍 SemiBold/600、非活躍 Normal/400 | 細 | 低 | 完成 |
| frame-page-panels-legacy-white | 工作空間頁面容器面板（m_plater/m_monitor/m_multi_machine/m_project/m_calibration）透過舊版 ThemeColor::White 設定 bg（依賴深色交換地圖），唔係 MD3 角色 | App.jsx:87（body bg surface-dim）；spec-navigation-shell.md §12 | MainFrame.cpp:1625 | 將 ThemeColor::White 替換為 StateColor::semantic(MD3::Role::SurfaceDim)（或適當嘅 Surface 角色），移除框架頁面面板上嘅舊版記號 | 細 | 低 | 完成 |
| settings-navrail-horizontal-tabbar | *（已合併。睇合併備註）* |  | Preferences.cpp:1260 | *（已合併）* |  |  | 完成 |

**合併備註（呢個表面）：**
- **`tabbar-icons-raster-no-fill`（存活嘅規範列）** 吸收 widgets-containment `notebook-workspace-tab-raster-icons`（相同嘅 Notebook.cpp / Button.cpp 標籤圖示渲染路徑 + 相同嘅點陣圖→Material Symbols FILL 切換變更）。
- **`settings-navrail-horizontal-tabbar`（已移除）** → 合併到 **settings-params `prefs-nav-rail-replaces-horizontal-tabbar`**（相同嘅 `Preferences.cpp:1260` PreferenceTabbar、相同嘅「使用工具包 230px 垂直 NavRail 取代」變更；設定對話框係更具體嘅表面）。
- **`gizmorail-opengl-texture-atlas`（已移除）** → 合併到 **prepare `gizmo-rail-opengl-not-md3`**（相同嘅 GLCanvas3D 模型軌 + 相同嘅重新皮膚到工具包模型軌變更；模型軌住喺準備工作空間裏面）。*（列已從呢個表格中刪除；合併記錄喺呢度。）*

---

## 表面：prepare（3D 編輯器工作空間）

| ID | 舊版元素 | 工具包參考 | 原生錨點 | 需要嘅變更 | 大細 | 風險 | 狀態 |
|----|---------|----------|---------|-----------|------|------|--------|
| gizmo-rail-opengl-not-md3 | 左邊模型軌係 OpenGL 垂直 GLToolbar：浮動（VO_Center）、5px 邊框/4px 間隔、點陣圖背景紋理 + SVG 圖譜圖示、DPI 縮放；GLToolbar/GLGizmosManager 裏面零 MD3 參考 | Prepare.jsx:34-39 + navigation/GizmoRail.jsx | GLCanvas3D.cpp:7884；GLToolbar.cpp:1666 | 取代（或重新皮膚到）固定 60/50px 左軌：bg SurfaceContainerLow、右 1px OutlineVariant、pad 8/gap 3、44x44 r12 按鈕（Primary 填充 + OnPrimary + FILL-1 已選取、透明 + OnSurfaceVariant + FILL-0 閒置）、21px Material Symbols、28x1 OutlineVariant 分隔符（open_with/rotate_right/open_in_full/flip/align_horizontal_left/carpenter/foundation/line_start_circle/brush/match_case/straighten） | 大 | 高 | 完成 |
| top-scene-toolbar-opengl-not-md3 | 頂部場景命令係水平 OpenGL 主 GLToolbar（HO_Right/VO_Top、紋理圖譜圖示/背景）；冇 MD3、位置錯誤（頂右對比工具包中心）、舊版項目集 | Prepare.jsx:42-44 (SCENE_TOOLS) | GLCanvas3D.cpp:7867 | 建置置中浮動藥丸：top:12 translateX(-50%)、bg SurfaceContainer、1px OutlineVariant、r16、elev-3、gap 3/pad 5、方形 40px IconButtons（r10、22px）使用 center_focus_strong/grid_view/auto_fix_high/content_copy/delete/undo/redo，或重新皮膚 p_main_toolbar 到呢個幾何/顏色/字形集 | 大 | 高 | 完成 |  <!-- 2026-07-24: 舊狀態。GLCanvas3D::get_main_toolbar_offset() 將藥丸置中喺畫布上，預留 2x 摺疊工具欄寬度以避免碰撞；已喺全新拍攝入面驗證（docs/screenshots/main-window/sidebar-prepare.png，藥丸置中喺檢視區域頂部）。 -->
| process-legacy-paramspanel-tree | 處理部分嵌入完整舊版 ParamsPanel 參數樹（密集設定標籤面板），冇 MD3 SectionHeader 同埋冇 MD3 欄位小工具 | Prepare.jsx:105-112 (Process card) | Plater.cpp:3156 | 取代為緊湊處理卡：SectionHeader tune；SelectField（預設）；SegmentedControl 【質素/強度/支撐/其他】；精心挑選嘅參數列（NumericField 用於數字、SelectField 用於枚舉、Switch 用於布林值）；「進階設定」文字按鈕打開完整參數。需要新嘅 MD3 ValueField/SelectField/SegmentedControl/Switch | 大 | 高 | 完成 |
| filament-rows-preset-combobox-not-inforow | 每列墨水係 PlaterPresetComboBox + clr_picker 樣本 + 點陣圖 ScalableButton('menu_filament') 編輯按鈕；冇圓角列容器、冇材料徽章、冇槽副標題 | Prepare.jsx:91-100（墨水列 + 徽章） | Plater.cpp:3306 | 重新建置為工具包資訊列：h44、bg SurfaceContainerHighest、r12、pad 0/8、gap 10；28x28 r8 顏色樣本（資料、保持）帶著內嵌圈；名稱 12.5/500 省略號 + 槽 10.5 OnSurfaceVariant；尾部材料型態徽章（11/600、SecondaryContainer、r7）；將預設選擇移動到點擊/功能表 | 大 | 中等 | 完成 |
| objects-legacy-searchctrl-dataviewctrl | 物體係原始 wxSearchCtrl（原生按鈕）+ 舊版 ObjectList wxDataViewCtrl；冇 MD3 SectionHeader、SearchField 或籌碼列 | Prepare.jsx:116-126（物體卡） | Plater.cpp:3171 | 提供工具包物體卡：SectionHeader account_tree；MD3 SearchField；選定列作為 SecondaryContainer/OnSecondaryContainer 籌碼（h36 r10、deployed_code 18、名稱 12.5/500、尾部可見性 17 + 16x16 r5 Primary 複選框）；「層同埋高度範圍」子列；重新樣式/包裝 ObjectList | 大 | 中等 | 完成 |  <!-- 2026-07-24: 舊狀態。Plater 建置工具包物體卡（SectionHeader account_tree + 共用 SearchField、Plater.cpp:3744）同埋 GUI_ObjectList 將選定列繪製為 SecondaryContainer 籌碼（GUI_ObjectList.cpp:93）；已喺波 10-14 + 審計修正中著陸。 -->
| slice-print-buttons-brandgreen-literals-wrong-anatomy | 切片/打印係舊版 SideButtons，使用硬編碼 BrandGreen/Hovered/Pressed 字面、radius 12、每個配對舊版點陣圖 'sidebutton_dropdown' 嘅下拉 SideButton；冇領先 Material Symbol | Prepare.jsx:71-72（切片已標示 / 打印已填充提升） | MainFrame.cpp:2834 | 切片 → 已標示（bg SurfaceContainerHigh、1px Outline、OnSurface、領先 deployed_code、h44、r=h/2）。打印 → 已填充提升（Primary/OnPrimary、領先 print FILL-1、elev-2、h44、r~22）。移除 BrandGreen 字面；重新考慮下拉插記號（工具包冇）| 中等 | 中等 | 完成 |
| printer-identity-card-combobox-anatomy | 打印機身份卡係 PlaterPresetComboBox + 點陣圖編輯/連接 ScalableButtons + 一個 48x48 縮圖、MinSize 68px，唔係靜態身份列（卡 bg 已經 sc-highest） | Prepare.jsx:78-85（打印機身份卡） | Plater.cpp:2523 | 符合工具包：52x52 r12 縮圖喺 SurfaceContainerLowest 上帶住打印字形（30）+ 1px OutlineVariant；名稱 13.5/600 省略號；狀態列 7x7 Primary 點 + '0.4 噴嘴 / 已連接' 11.5 Primary；尾部編輯 IconButton 34；pad 10（移除 68px）；保留組合框喺編輯惠及後面 | 中等 | 中等 | 完成 |
| bed-card-84px-vs-selectfield | 打印板類型係 84px StaticBox 卡（sc-high），帶住打印板縮圖、ComboBox、點陣圖幫助 ScalableButton 同埋 Body_11 標題 | Prepare.jsx:86（SelectField '打印板類型'） | Plater.cpp:2571 | 摺疊為單個已標示 SelectField（標題「打印板類型」、值例如「紋理 PEI 打印板」、h38、1px Outline、r10）；移除 84px 卡、縮圖同埋幫助按鈕（將幫助移動到欄位惠及） | 中等 | 中等 | 完成 |
| section-headers-legacy-titlebar-raster-icons | 打印機/墨水標題係可摺疊 StaticBox 標題列，帶住點陣圖 ScalableButton 領先圖示 + 尾部點陣圖設定、最小高度 3*em；處理/物體/物體操作冇標題；標籤文字「項目墨水」（工具包：「墨水」） | Prepare.jsx:77,90,105,116,130 + containment/SectionHeader.jsx | Plater.cpp:2457 | 介紹共用 MD3 SectionHeader（16px 領先 Material Symbol print/palette/tune/account_tree/transform、11/600 +.6px OnSurfaceVariant、可選尾部控制、冇固定 3em 欄），應用到所有五個部分；改名「項目墨水」 → 「墨水」；用 MaterialIcon 替換點陣圖 ScalableButton 字形 | 中等 | 低 | 完成 |
| object-manipulation-sidebar-card-absent | 工具包側欄物體操作 X/Y/Z 欄位卡不存在原生；位置/旋轉/縮放/大細編輯只喺 ImGui 模型疊加層入面 | Prepare.jsx:130-140（物體操作欄位） | Plater.cpp:3213 | 新增側欄卡：SectionHeader transform；4 欄網格帶住軸彩色 X/Y/Z 標題（10.5/600 喺 axis-x/y/z）同埋位置/旋轉/縮放%/大細列，每個儲存格 h32 r8 sc-highest 置中 Roboto Mono 12；綁定到與 ImGui 面板相同嘅模型 | 中等 | 中等 | 完成 |
| viewport-overlays-zoom-cluster-statpill-axis | 冇 MD3 縮放叢集或物體統計藥丸作為檢視區域 chrome；軸指示器由舊版 GL 路徑繪製，唔係工具包疊加層使用 axis-* 記號 | Prepare.jsx:45-58（軸模型、縮放叢集、物體統計藥丸） | GLCanvas3D.cpp（檢視區域渲染）；MD3Tokens.hpp:367 | 新增工具包疊加層：底右縮放叢集（40px add/remove/filter_center_focus IconButtons 嘅欄喺 r26 SurfaceContainer 卡裏面、elev-3）、底中物體統計藥丸（pad 7/14、r20、12.5 OnSurfaceVariant、deployed_code 16）、對齊軸顏色到 axis-x/y/z（資料軸顏色保持；只有卡/藥丸 chrome 喺範圍裏面） | 中等 | 中等 | 完成 |
| filament-subtitle-row-legacy-raster-buttons | 舊版「墨水」副標題列帶住 Body_14 標籤、手繪分隔符同埋點陣圖 ScalableButtons add_filament/delete_filament/ams_fila_sync/settings，冇一個喺工具包解剖入面 | Prepare.jsx:90（同步 AMS 作為標題尾部按鈕） | Plater.cpp:2881 | 移除副標題列；將 AMS 同步移動到墨水 SectionHeader 尾部槽作為已標示 MD3 按鈕（h30、11.5、Primary、sync 字形）；將新增/刪除/設定折疊到列/功能表模型 | 中等 | 低 | 完成 |
| plate-chip-missing-grid-view-glyph | 打印板籌碼（SecondaryContainer 填充、2px Primary 邊框、r12、顏色正確）冇領先圖示 | Prepare.jsx:62-63（打印板籌碼、領先 grid_view） | MainFrame.cpp:2148 | 將領先 grid_view Material Symbols 字形（20px）新增到打印板籌碼 | 細 | 低 | 完成 |
| add-plate-plus-text-vs-glyph | 新增打印板按鈕使用字面「+」文字標籤（虛線輪廓/r12 已經正確），而唔係新增字形 | Prepare.jsx:64（新增打印板帶著新增字形） | MainFrame.cpp:2149 | 將「+」文字替換為 20px Material Symbols「add」字形（OnSurfaceVariant），保持虛線 1px Outline、透明填充、r12 | 細 | 低 | 完成 |
| estimate-block-single-font-no-length | 估計係一個 wxStaticText，Mono_12 用於兩行並且省略墨水長度；工具包列 1 mono 15/500、列 2 11px 非 mono OnSurfaceVariant 帶住「重量 / 長度」| Prepare.jsx:67-70（2 層估計） | MainFrame.cpp:2182 | 渲染兩層：列 1 Roboto Mono 15/500（時間）、列 2 11px OnSurfaceVariant（重量 g / 長度 m）；擴展 update_prepare_action_bar_content 以包括長度 | 細 | 低 | 完成 |
| action-bar-residual-raster-chrome-helio-splitline | 動作欄帶住點陣圖「topbar_line」分隔符點陣圖 + 點陣圖 Helio/expand 圖示嘅 ExpandButtonHolder，舊版點陣圖 chrome，冇工具包類比 | Prepare.jsx:60-73（動作欄 = 籌碼/新增/估計/切片/打印只有） | MainFrame.cpp:2217 | 如果保留，重新樣式到 MD3（Material Symbols IconButtons + 1px OutlineVariant 分隔符，而唔係 topbar_line）；否則從工具包動作欄重新放置。欄裏面冇點陣圖點陣圖 | 細 | 低 | 完成 |
| sync-info-button-not-in-kit-printer-section | 一個獨立「同步資訊」按鈕帶住點陣圖「printer_sync」圖示住喺打印機部分；工具包只喺墨水標題上放置 AMS 同步 | Prepare.jsx:77-86（打印機部分冇獨立同步資訊按鈕） | Plater.cpp:2661 | 根據工具包移除或重新放置同步資訊控制；如果保留，使用 Material Symbols 字形同埋工具包按鈕解剖 | 細 | 低 | 完成 |
| filament-title-purge-flush-aux-buttons | 兩個輔助按鈕（「淨化模式」、「沖洗體積」）住喺墨水標題欄，Body_10 / r8，非工具包解剖同埋不合 MD3 類型量表 | Prepare.jsx:90-101（墨水部分：標題 + 列 + 只新增墨水） | Plater.cpp:2795 | 將呢啲進階控制移出部分標題（喺進階/溢流後面）或重新樣式到工具包按鈕解剖（MD3 類型量表、藥丸/工具包半徑） | 細 | 低 | 完成 |
| add-filament-footer-raster-icon | 全寬「新增墨水」頁尾按鈕（顏色/半徑已經 MD3 已標示）使用點陣圖「add_filament」點陣圖同埋 Body_12 | Prepare.jsx:101（新增墨水、已標示、新增字形） | Plater.cpp:2992 | 交換點陣圖 add_filament 圖示為 Material Symbols「add」字形，並將標籤對齊到工具包主體大細；保持已標示 Primary 處理 | 細 | 低 | 完成 |

**合併備註（呢個表面）：** **`gizmo-rail-opengl-not-md3`（存活嘅規範列）** 吸收 chrome-nav `gizmorail-opengl-texture-atlas`（相同嘅 GLCanvas3D 模型 GLToolbar + 相同嘅重新皮膚到工具包變更）。相同軌嘅圖示管道關注（icons-assets `gizmo-rail-svg-icons`、每個模型字形→GL 紋理橋）係一個*相異*列，不同錨點（`GLGizmoMove.cpp:121` 等。）同埋唔同變更（圖示渲染路徑），同埋保留；呢兩個喺 GL 重組波中共同排程。

---

## 介面：預覽（G-code 預覽層疊）

| ID | 遺留元件 | 工具包參考 | 本機錨點 | 所需變更 | 大細 | 風險 | 狀態 |
|----|---------|----------|---------|---------|------|------|------|
| preview-timeline-bar-missing | 底部嘅「移動」滑桿係透明浮動 ImGui 視窗，淨係繪製凹槽、圓形手柄同浮動值標籤；冇不透明時間軸帶同冇傳輸控制 | Preview.jsx:36-42 (move timeline bar); actions/IconButton.jsx | IMSlider.cpp:1057 | 建構 58px 時間軸帶：不透明 SurfaceContainerLow 全寬、頂部 1px OutlineVariant、邊距 0/20；傳輸 = skip_previous (38) + 圓形播放 (44x44 r50%, Primary/OnPrimary play_arrow FILL-1) + skip_next (38) + 範圍滑桿 (flex 1) + 右對齊等寬「移動 cur/max」(12, OnSurfaceVariant, min-w120)。需要播放動作 + ImGui 裏嘅 Material Symbols 圖示字型 | 大 | 高 | 完成 |
| preview-options-icon-chips | 旅行/縫線/回縮/無回縮/墨水變更/擦拭呈現為密集表格行，具有樣本 + 「顯示」欄中嘅核取方塊；冇圖示 | Preview.jsx:60-64 + optIcons:16; selection/Chip.jsx | BaseRenderer.cpp:2149 | 將選項呈現為可選擇嘅 MD3 圖示晶片：旅行 (route)/縫線 (line_start_circle)/回縮 (u_turn_left)/擦拭 (water_drop)；已選擇 Primary/OnPrimary、未選擇透明 + Outline + OnSurfaceVariant、h30 r=h/2、前導 Material Symbol @16、環繞；前面有「調整」SectionHeader | 中等 | 中等 | 完成 |
| preview-viewmode-chip-layout | 檢視模式晶片喺 BeginTable(SizingStretchSame) 裏佈置，每個 Button 寬度 -FLT_MIN (伸展至相等欄)；工具包晶片係內聯伸縮、貼著內容、環繞 (晶片 COLOR 令牌已正確) | Preview.jsx:46-49; selection/Chip.jsx | BaseRenderer.cpp:1884 | 將伸展表格取代為內聯內容貼著環繞晶片列 (SameLine + wrap、邊距 0/13)，使晶片大細適應其標籤 | 中等 | 中等 | 完成 |
| preview-statistics-typography | 卡面正確，但：標題使用 imgui.title() 粗體標題格式而唔係 SectionHeader 帶「insights」圖示；數值使用比例 imgui.text 而唔係 Roboto Mono 500；卡淨係喺 show_estimated 時出現，但工具包喺切片時始終顯示統計資訊 | Preview.jsx:66-74 (Statistics card); containment/SectionHeader.jsx | BaseRenderer.cpp:2713 | 標題 → MD3 SectionHeader (11/600/uppercase/+.6px, 「insights」圖示)；數值喺 Roboto Mono 500 ImGui 字型裏、右對齊 OnSurfaceVariant 標籤；喺存在切片時顯示統計資訊 | 中等 | 中等 | 完成 |
| preview-imgui-mono-font | ImGui 圖集裏冇登記等寬字型；層疊裏嘅每個數值/技術值都係用比例圖集字型呈現 | type-mono.html; MD3Tokens.hpp Type::font_mono 'Roboto Mono' | ImGuiWrapper.cpp:1 | 將 Roboto Mono ImGui 字型 (+ 粗體) 登記到圖集，並公開 push/pop 幫手程式，使時間軸計數器、統計值同層滑桿 Z 標籤呈現為等寬 500。阻擋 preview-timeline-bar-missing、preview-statistics-typography、preview-vertical-slider-z-chip | 中等 | 中等 | 完成 |
| preview-color-scheme-sectionheader | 「色彩配置」通過 imgui.bold_text() 呈現，粗體標題格式主體字型、冇圖示 | Preview.jsx:46; containment/SectionHeader.jsx | BaseRenderer.cpp:1807 | 呈現為 MD3 SectionHeader：11px / 600 / uppercase / +.6px、OnSurfaceVariant、前導「palette」Material Symbol @16 | 細 | 低 | 完成 |
| preview-section-titles-legend | 圖例表格標題列 (線型/時間/百分比/已用墨水/顯示) 用 imgui.bold_text() + Separator() 繪製；冇 SectionHeader 樣式嘅章節標題；當前配置名稱從未顯示為章節標籤 | Preview.jsx:52,46,61,67; containment/SectionHeader.jsx | BaseRenderer.cpp:1630 | 匯入 SectionHeader 樣式 (11/600/uppercase/+.6px、OnSurfaceVariant) 嘅章節標題，並將當前配置名稱顯示為 SectionHeader 喺圖例清單之上 | 中等 | 中等 | 完成 |
| preview-legend-swatch-geometry | 圖例色樣係尖角填充正方形 (~0.7 x 行高、冇圓角) 通過 AddRectFilled | Preview.jsx:55 (swatch 16x16 r4) | BaseRenderer.cpp:1543 | 將樣本繪製為 16x16 (縮放) 圓角矩形、r4；填充色係資料並保持 | 細 | 低 | 完成 |
| preview-slicing-result-title | imgui.bold_text('Slicing Result') 呈現粗體標題格式停靠標題，冇工具包對應項 | Preview.jsx:44-49 (sidebar opens on 'Color scheme' SectionHeader, no dock title) | BaseRenderer.cpp:1774 | 移除/重新佈置「Slicing Result」標題，使停靠喺「色彩配置」SectionHeader 上打開；保留折疊/展開控制 | 細 | 低 | 完成 |
| preview-gcode-window-syntax-colors | 循序 G-code 文字面板使用硬編碼灰色/白色作為 COMMAND/PARAMETERS/COMMENT 色，同硬編碼陰影矩形 ImVec4(0,0,0,0.3) | colors.css OnSurface/OnSurfaceVariant + scrim | BaseRenderer.cpp:3606 | 令牌化：命令/參數/評論文字 → OnSurface / OnSurfaceVariant 步驟；用 MD3::scrim 取代 0.3 調光矩形 | 細 | 低 | 完成 |
| preview-slider-tooltip-legacy | 層滑桿刻度線提示推送 PopupBg = COL_WINDOW_BACKGROUND (遺留) 同文字 = 硬編碼白色 | colors.css SurfaceContainer / OnSurface | IMSlider.cpp:795 | 令牌化提示：bg SurfaceContainer、文字 OnSurface、邊框 OutlineVariant | 細 | 低 | 完成 |
| preview-recommendation-overlay-tints | 多噴嘴分組建議子項使用遺留硬編碼著色：ChildBg ImVec4(0.3,0.3,0.3,0.1) 同 (0,0,0,0.1) + IM_COL32(0,0,0,20) 著色矩形 | colors.css surface-container steps / scrim | BaseRenderer.cpp:3032 | 用 MD3 SurfaceContainer 步驟色 (或 scrim 令牌) 解析為活躍佈景主題取代硬編碼灰色/黑色著色 | 細 | 低 | 完成 |
| preview-vertical-slider-z-chip | 垂直層滑桿值標籤呈現喺 SurfaceContainerLow 晶片 (~2px 圓角) 具有比例字型；工具包「Z 24.80 mm」晶片使用 bg SurfaceContainer、r8、等寬 | Preview.jsx:27 (Z chip: bg sc, r8, mono) | IMSlider.cpp:865 | 樣式值晶片：bg SurfaceContainer、r8、值喺 Roboto Mono (取決於 preview-imgui-mono-font) | 細 | 低 | 完成 |
| preview-one-layer-raster-icons | 單層/所有層切換載入光柵 SVG 紋理 (one_layer_on/off + *_hover/_dark) 同用 ImageButton3 呈現；無圖示 | icons.html; MD3Tokens.hpp Type::font_icon 'Material Symbols Outlined' | IMSlider.cpp:148 | 將光柵 SVG 紋理按鈕取代為 Material Symbols 字型圖示按鈕 (例如 layers/stack)；配合圖示按鈕鉻合 (幽靈圓形、OnSurfaceVariant、sc-high 懸停) | 細 | 低 | 完成 |
| preview-viewport-status-pill | 預覽檢視區冇左上狀態藥丸 (工具包顯示「Sliced · N layers」) | Preview.jsx:32-34 (top-left status pill) | BaseRenderer.cpp:1452 | 新增左上檢視區狀態藥丸：bg SurfaceContainer、r20、1px OutlineVariant、elev-2、前導「layers」@16、12.5 OnSurfaceVariant 文字 (加法同位性) | 細 | 低 | 完成 |

---

## 介面：裝置 (裝置/監測工作區 + 多機 + 傳送選擇器)

| ID | 遺留元件 | 工具包參考 | 本機錨點 | 所需變更 | 大細 | 風險 | 狀態 |
|----|---------|----------|---------|---------|------|------|------|
| device-pause-stop-md3-buttons | 暫停/繼續同停止係無邊框光柵 ScalableButtons (print_control_pause/resume/stop PNGs)；冇藥丸幾何、冇聲調/危險令牌填充、冇字型 | Device.jsx:42-45 (tonal pause + danger stop, lg h44 r22) | StatusPanel.cpp:1150 | 暫停 → 聲調 (secondary-container) 帶「pause」字型；停止 → 危險 (透明 + 1px 錯誤邊框、錯誤文字) 帶「stop」字型；兩者 lg (h44, r=h/2)、flex:1；保留暫停/繼續狀態切換 | 中等 | 中等 | 完成 |
| device-progress-title-strip | PrintingTaskPanel 呈現全寬「打印進度」標題帶 (device_title_color、PAGE_TITLE_HEIGHT) 喺縮圖列上方 | Device.jsx:32-46 (progress Card has no title strip) | StatusPanel.cpp:1034 | 移除標題帶解剖；直接折疊縮圖 + 名稱/副 + 百分比到卡身體 | 細 | 低 | 完成 |
| device-progress-metadata-raster-icons | 時間/重量後設資料繪製為光柵 wxStaticBitmaps (m_bitmap_use_time/_use_weight)；縮圖預留位置光柵「monitor_placeholder」| Device.jsx:36-37 (plain-text sub, 60x60 r12 thumbnail with deployed_code) | StatusPanel.cpp:1068 | 捨棄光柵時間/重量圖示以取得工具包嘅單一 OnSurfaceVariant 後設資料副行 (或 Material Symbols 如果保留)；將閒置縮圖呈現為圓角 12 磚帶「deployed_code」字型 | 細 | 低 | 完成 |
| device-camera-hud-raster-status-glyphs | 相機狀態指示器喺合規 CameraHUD 內 (sdcard/timelapse/recording/vcamera) 係光柵深色背景 PNGs | Device.jsx:18-30 (overlay chips use vector glyphs) | StatusPanel.cpp:2108 | 將四個 HUD 狀態指示器遷移到 Material Symbols 字型 (通過 MaterialIcon) 喺固定深色 HUD 前景 | 中等 | 低 | 完成 |
| device-mediaplayctrl-bar-anatomy-and-raster | 單獨 40px MediaPlayCtrl 帶坐喺 wxMediaCtrl 下方；其播放/停止控制係光柵 Button (media_play/media_stop PNGs)；工具包將相機控制折入卡層疊 | Device.jsx:18-31 (camera card self-contained; overlay controls only) | MediaPlayCtrl.cpp:135 | 交換光柵 media_play/stop 為 Material Symbols play/stop；長期集成播放/狀態親和性到相機卡層疊 (色已 MD3) | 中等 | 中等 | 完成 |
| device-control-title-strip-and-action-buttons | 控制欄開始遺留「控制」標題帶 (device_title_color) 帶打印機零件/打印選項/安全選項/校準藥丸按鈕 | Device.jsx:48-95 (right column is cards only, no 'Control' header) | StatusPanel.cpp:2205 | 移除標題帶；將仍需要嘅零件/校準進入點遷移到卡級別動作或選單，使欄變成純 MD3 卡堆 | 大 | 高 | 完成 |
| device-temperature-rows-raster-and-anatomy | 噴嘴/打印板/室溫行係 TempInput 複合，具光柵 PNG 圖示 (monitor_nozzle/bed/frame_temp[_active]) + 內聯可編輯欄 (48-52 高)；唔係工具包圖示 + 標籤 + 等寬值 + 編輯列 | Device.jsx:49-60 (icon 22 + label + mono value + mono target + 32px edit IconButton) | StatusPanel.cpp:2390 | 用 22px 青色 Material Symbols (mode_heat/radio_button_checked/home_work) 取代 TempInput 光柵圖示；當前值 Roboto Mono 15/500、目標等寬 12 OnSurfaceVariant；將編輯移到尾隨 32px 填充編輯 IconButton 後 | 大 | 中等 | 完成 |
| device-section-headers-missing-glyphs | 所有四個右欄卡標題 (溫度/打印選項/移動/墨水機) 係純大寫 Head_11、冇前導 Material Symbol 同冇字母間距；墨水機標題冇青色「濕度」尾隨 | Device.jsx:50,62,69,81 (SectionHeader = 16px glyph + 11/600 +.6px; AMS teal 'Humidity: Dry' trailing) | StatusPanel.cpp:2316 | 給每個標題前導 16px Material Symbol (thermostat/speed/control_camera/inventory_2)、應用 SectionHeader 字母間距、新增青色「濕度：狀態」尾隨標籤到墨水機標題 | 中等 | 低 | 完成 |
| device-print-options-segmented-sliders-switch | 打印選項係遺留 ImageSwitchButton 速度切換 (monitor_speed[_active]) + ImageSwitchButton 燈切換 (monitor_lamp_on/off) + FanSwitchButton (monitor_fan_on/off)，冇符合工具包分段/滑桿/開關 | Device.jsx:61-67 (4-way SegmentedControl + two teal sliders + teal Switch) | StatusPanel.cpp:2467 | 重組：4 路 SegmentedControl 速度 (活躍青色)、青色強調範圍滑桿以供零件冷卻 + 輔助風扇、青色開關以供室溫燈；捨棄光柵 monitor_speed/lamp/fan PNGs | 大 | 高 | 完成 |
| device-move-xy-axis-dial-vs-grid | XY 控制係遺留圓形 AxisCtrlButton 撥盤 (258x258)，帶弧線繪製標籤同 +10/+1/-1/-10 環；只有中心主頁字型被遷移 | Device.jsx:79-89 (3x3 40px arrow grid, keyboard_arrow_* glyphs, home center on secondary-container) | StatusPanel.cpp:2573; Widgets/AxisCtrlButton.cpp | 用 3x3 40px 箭頭網格取代圓形撥盤 (sc-highest 磚 r8、keyboard_arrow_up/left/right/down) + 中心主頁按鈕喺 secondary-container | 大 | 高 | 完成 |
| device-move-z-extruder-raster-icons | Z/打印板按鈕使用光柵 monitor_bed_up/down 喺水平打印板帶；擠出機欄使用光柵 monitor_extruder_up/down；解剖係水平打印板列 + 獨立擠出機冊 | Device.jsx:90-93 (vertical list of sc-highest r10 rows: Z +10/-10 + Extrude) | StatusPanel.cpp:2604 | 交換光柵 monitor_bed_*/extruder_* PNGs 為 Material Symbols (keyboard_arrow_up/down、vertical_align_bottom) 並重組為工具包垂直 Z 軸清單，sc-highest r10 列帶文字標籤 | 大 | 中等 | 完成 |
| device-ams-card-legacy-widget | 墨水機卡內裏係遺留 AMSControl 小工具 (0 MD3 令牌；~25 AMS_CONTROL_* 遺留調色板參考)，完整遺留解剖；淨係外部 StaticBox 係青色主題 | Device.jsx:68-78 (horizontal strip of per-slot sc-highest r12 tiles, 30px swatch r8, 10px type label) | StatusPanel.cpp:2747; Widgets/AMSControl.cpp | 作為監測同位，將墨水機卡主體呈現為工具包樣本帶 (每槽 sc-highest r12 磚、30px 墨水樣本 r8、類型標籤)，使用 MD3 令牌；保留樣本色 (資料)。大因為 AMSControl 同時驅動裝載/卸載 | 大 | 高 | 完成 |
| device-hms-tab-unmigrated | HMS (助手) 標籤完全未遷移：SetBackgroundColour(*wxWHITE)、內容前景 *wxBLACK、wxColour(238,238,238) 分隔線/背景；冇 MD3 令牌 | colors.css (MD3 surface/on-surface/outline-variant) | HMSPanel.cpp:28 | 將背景/文字/分隔線遷移到 device_* / 語義 MD3 角色 (SurfaceContainerLow 卡、OnSurface 文字、OutlineVariant 線) 以供淺色 + 深色 | 中等 | 低 | 完成 |
| device-storage-tab-white-panel | 儲存標籤 (MediaFilePanel) 類型選擇器面板使用 *wxWHITE StateColor 同 SetBackgroundColor(*wxWHITE)；否則部份遷移 | colors.css (surface roles; no bare white panels) | MediaFilePanel.cpp:48 | 用適當嘅 MD3 表面角色 (裝置卡/SurfaceContainerLow) 取代 *wxWHITE 類型面板 bg/StateColor | 細 | 低 | 完成 |
| device-multi-farm-list-vs-card-grid | 多裝置農場係單一垂直 MultiMachineItem 列清單，具自訂繪製內聯進度，冇每個裝置相機縮圖卡；色 MD3 但解剖係清單 | Multi.jsx:19-31 (responsive Card grid; per card icon + name/model + status dot + camera thumb + progress) | MultiMachineManagerPage.cpp:423; MultiMachinePage.cpp:194 | 重組為回應式卡網格：每個裝置一張 MD3 卡，帶打印機圖示磚、名稱/型號、狀態點、相機縮圖預留位置、MD3 進度條 | 大 | 高 | 完成 |
| device-selectmachine-popup-legacy-literals | 傳送到裝置選擇器 + EditDevNameDialog 使用遺留文字：*wxWHITE 章節面板、wxColour(147,147,147) 標題、*wxWHITE 重命名 bg、wxColour(166,169,170) 線、wxColour(255,111,0) 警告、同 Bambu 綠色確認 (0,174,66)/(27,136,68) 帶白色文字 | actions/*.prompt.md (MD3 primary fill, not Bambu green) + colors.css | SelectMachinePop.cpp:467 | 用 device/surface 角色取代白色 bg、用 OnSurfaceVariant/OutlineVariant 取代灰色標題/線、用 MD3 警告令牌取代警告文字、用 MD3 主要 (品牌/裝置) 按鈕令牌取代 Bambu 綠色確認 (鉻色、非墨水資料) | 中等 | 低 | 完成 |

---

## 介面：settings-params（偏好設定對話框 + 預設編輯器 / 參數）

| 編號 | 舊元件 | 套件參考 | 原生錨點 | 必要改動 | 大小 | 風險 | 狀態 |
|----|----------------|---------|---------------|-----------------|------|------|--------|
| prefs-nav-rail-replaces-horizontal-tabbar | PreferenceTabbar 係一個水平文字標籤欄，綠色底線疊喺 Grey300 分割線上面；有 4-5 個文字標籤，冇前導圖示 | Settings.jsx:20-22（230px 軌道欄、sc-low、右邊 OutlineVariant、pad 16/10）+ navigation/NavItem.jsx | Preferences.cpp:1260 | 換成固定嘅 230px 垂直 NavRail（bg SurfaceContainerLow、右邊 1px OutlineVariant、pad 16/10、gap 2），NavItem 藥丸（h44 r22 gap10、20px 前導字形、已選 SecondaryContainer/OnSecondaryContainer 600、懸停 High、閒置 OnSurfaceVariant 400）；將各節對應到外觀/一般/預設/網絡/版本控制/關於，加上字形；加 settings_nav_w=230 到 Metrics | large | medium | done |
| prefs-appearance-section-missing | 主題控制係 Win32 限制嘅 ::CheckBox「啟用深色模式」；冇密度控制，冇強調色控制；唔存在「外觀」節 | Settings.jsx:26-47（主題 + 密度 SegmentedControls、強調色樣本行）+ native-settings-filament.md §3.4 | Preferences.cpp:888 | 建立一個外觀節：主題 = md SegmentedControl 淺色/深色（跨平台、綁定 dark_color_mode）；密度 = md SegmentedControl 舒適/緊湊（需要新嘅「ui_density」鍵 + 執行時 Metrics 選擇器）；強調色 = 樣本行，有 32px 圓形（需要強調色種子基礎設施）。需要新嘅原生 SegmentedControl | large | high | done |
| prefs-white-backgrounds-no-surface-layering | 對話框、標籤欄同埋每個 ScrollPanel 硬編碼咗 ThemeColor::White（只能透過舊交換變深色）；nav 軌道欄同內容之間冇 surface-container 分層 | Settings.jsx:19-23（根部 surface、nav sc-low、內容面板） | Preferences.cpp:1231 | 移除硬編碼嘅 White；根據語義 semantic(Surface) 驅動內容面板背景同埋 SurfaceContainerLow 驅動 nav 軌道欄背景，咁樣深色就會按角色解析 | medium | low | done |
| prefs-row-builders-legacy-colors-fonts-layout | 節標題 Head_13 喺 TextSecondary（48px pad）；偏好設定行單行 ::CheckBox + wxStaticText（TextPrimary/Body_13），有 64px 縮排；::SwitchButton（r10）唔係套件 Switch，佢嘅處理程式係 no-op 樁 | Settings.jsx:50-59（節標題 16/700；雙行行、右對齊 Switch）+ selection/Switch | Preferences.cpp:112 | 透過 OnSurface/OnSurfaceVariant 重新著色標籤；節標題 16/700；將「一般」偏好設定重組成雙行行（主要 13.5/500 + 副要 12/OnSurfaceVariant），右對齊 MD3 Switch，改樣式成 44x24 r14 | large | medium | done |
| prefs-value-select-field-chrome | 數值輸入使用 Grey250/White StateColor bg 加舊 TextInput；combobox 行使用 ::ComboBox + TextPrimary/Body_13 標籤；都唔符合套件 ValueField/SelectField | fields/ValueField.jsx + SelectField.jsx | Preferences.cpp:662 | 改樣式值輸入成 ValueField（sc-highest 填充、r10、單間距值 + OnSurfaceVariant 單位）同埋下拉式選單成 SelectField（sc-highest 或 Outline r10、尾部 expand_more 字形）；移除 Grey250/White 字面值 | medium | low | done |
| prefs-bottom-buttons-legacy-geometry | 「重設所有警告對話框」/「重設偏好設定」按鈕使用 Grey300/Grey400/BrandGreenHovered 字面值，有 r6 同埋 Body_13，唔係 MD3 按鈕變數 | actions/Button.jsx（藥丸 r=h/2；outline = transparent + 1px Outline + OnSurface） | Preferences.cpp:2003 | 改樣式成 MD3 outline（或 tonal）按鈕：藥丸半徑、transparent/Outline 邊框加 OnSurface 文字（或 SecondaryContainer tonal）、懸停 SurfaceContainerHigh，透過語義角色 | small | low | done |
| prefs-search-field-missing | 「偏好設定」對話框冇搜尋欄位 | Settings.jsx:25（SearchField「搜尋設定」）+ fields/SearchField.jsx | Preferences.cpp:1367 | 喺內容面板頂部加一個 MD3 SearchField 藥丸（h40 r22、sc-highest 填充、前導 20px 搜尋字形、佔位符「搜尋設定」） | medium | low | done |
| tab-editor-white-panel-backgrounds | 預設編輯器標籤同埋頂部工具列面板硬編碼咗 ThemeColor::White（依賴交換） | Settings.jsx:23（內容面板 surface） | Tab.cpp:173 | 用語義 semantic(Surface) 取代 ThemeColor::White 用於標籤本體同埋 SurfaceContainerLow 用於頂部工具列面板 | small | low | done |
| tab-editor-tabctrl-tree-and-raster-icons | 設定類別導航係舊 TabCtrl 樹控制（Body_14、固定 20*em、透過 UpdateDarkUI 交換變深色），每個類別嘅圖示係光柵點陣圖透過 wxImageList，唔係 MD3 NavItem 藥丸加字形 | navigation/NavItem.jsx + Settings.jsx 導航圖案 | Tab.cpp:500 | 將類別列表改皮成 MD3 藥丸 NavItems（h44 r22、已選 SecondaryContainer、懸停 High、閒置 OnSurfaceVariant），有 20px 物料符號前導字形取代光柵圖示；用角色驅動顏色而唔係 UpdateDarkUI | large | medium | done |
| tab-editor-search-field-geometry-glyph | 預設編輯器搜尋盒使用語義顏色但舊幾何：r5（套件 r22 藥丸）、冇前導搜尋字形（6px 間隔）、bold_font 唔係 13.5 正文 | fields/SearchField.jsx | Tab.cpp:323 | 符合 SearchField：h40、r22 藥丸、20px 前導「search」字形、body-13.5 字體；保留主色聚焦邊框 | medium | low | done |
| tab-editor-toolbar-raster-iconbuttons | 預設編輯器 + ParamsPanel 工具列控制（儲存/叉/搜尋/復原/復原到系統/處理/比較/表格）係舊光柵 ScalableButtons 透過圖示名稱載入，唔係 MD3 IconButtons 加字形 | actions/IconButton.jsx（圓形 + 物料符號） | Tab.cpp:283; ParamsPanel.cpp:278 | 用 MD3 IconButtons（圓形、懸停 SurfaceContainerHigh、OnSurfaceVariant）取代 ScalableButton 光柵圖示，透過 MaterialIcon 渲染儲存/關閉/搜尋/復原/compare_arrows/表格字形 | medium | medium | done |
| tipsdialog-button-brandgreen-literal-radius | TipsDialog 嘅肯定按鈕從 BrandGreen* 字面值填充，有 r12 而唔係主色 + 完整藥丸半徑（取消已經喺語義角色上） | actions/Button.jsx（填充 = 主色/OnPrimary、藥丸 r=h/2） | ParamsPanel.cpp:134 | 用語義 semantic(Primary)（OnPrimary 文字）驅動肯定填充，用藥丸半徑（高度/2），移除 BrandGreen 字面值 | small | low | done |

**合併備註（此介面）：** **`prefs-nav-rail-replaces-horizontal-tabbar`（倖存規範行）**吸收 chrome-nav `settings-navrail-horizontal-tabbar`（相同 `Preferences.cpp:1260` PreferenceTabbar、相同用套件垂直 NavRail 取代嘅改動）。

**已過時（標籤式設定）：**已被取代嘅 `PreferenceTabbar` NavRail 後來被共用瀏覽器風格 `Widgets/TabStrip` 停靠喺左邊（相同 230 px 軌道欄、相同 NavItem 藥丸處理、加上溢流 / 重新排序 / 釘住 / 分組 / 搜尋 / 批量關閉同埋 tablist 無障礙存取）。睇 `docs/features/workspace/tabbed-settings.md`。

---

## 介面：home-project（首頁 + 項目 webviews 同埋佢哋嘅原生 chrome）

| 編號 | 舊元件 | 套件參考 | 原生錨點 | 必要改動 | 大小 | 風險 | 狀態 |
|----|----------------|---------|---------------|-----------------|------|------|--------|
| modelmall-store-legacy-toolbar-chrome | 「3D 模型」商店視窗工具列：頂部分隔符 wxColour(166,169,170)（唔喺深色地圖入面）、m_web_control_panel + 返回/前進/重新整理按鈕 SetBackgroundColour(*wxWHITE)，三個導航按鈕係 ScalableButton 光柵資產（mall_control_back/forward/refresh） | colors.css（OutlineVariant 分隔符、SurfaceContainerLowest 填充）；Home.jsx + typography.css | ModelMall.cpp:29 | 分隔符 → OutlineVariant；控制面板 + 按鈕 bg → SurfaceContainerLowest；用 MaterialIcon arrow_back/arrow_forward/refresh 著色 OnSurfaceVariant 取代 ScalableButton 資產 | medium | low | done |
| home-online-toolbar-raster-icons | 首頁標籤線上 webview 工具列：bg 來自舊 darkModeColorFor(*wxWHITE)（深色地圖到 sc-low 唔係 sc-lowest）；返回/重新整理/喺瀏覽器開啟係光柵 create_scaled_bitmap 嘅自訂 SVG | colors.css（SurfaceContainerLowest）；native-chrome.md §8.E + typography.css | WebViewDialog.cpp:168 | toolbar_bg → SurfaceContainerLowest；透過 MaterialIcon arrow_back/refresh/open_in_new 渲染三個按鈕（保留 OnSurface 啟用 / OutlineVariant 停用著色） | small | low | done |
| home-plugin-banner-inset | 首頁左面板：「安裝網絡外掛程式」錯誤橫幅緊靠視窗邊緣（冇側邊邊距），有 5 px 內邊距，而每個導航項目都坐喺面板嘅 8 px 內縮；唯一首頁元件喺嘗試-11 擷圖之外 | colors.css（ErrorContainer / OnErrorContainer）；layout 間距尺度 | resources/web/homepage3/css/left.css `#NoPluginTip` | margin 6 px 頂部 / 8 px 側邊、padding 8 px / 12 px、radius 12 保留、錯誤角色未變 | small | low | done |
| login-dialog-legacy-native-parts | 登入對話框（ZUserLogin）：對話框 SetBackgroundColour(*wxWHITE)（兩次）；外掛程式缺失分支有 wxColour(166,169,170) 分隔符、*wxBLACK 訊息同埋舊藍色 wxHyperlinkCtrl；UpdateDlgDarkUI 只透過 wx 通用程式修復深色 | colors.css（Surface/SurfaceContainerLowest、OutlineVariant、OnSurface）；StateColor.hpp ThemeColor::Link | WebUserLoginDialog.cpp:45 | 對話框 bg → SurfaceContainerLowest；分隔符 → OutlineVariant；訊息文字 → OnSurface；用 ThemeColor::Link 樣式下載超連結 | small | low | done |
| project-webview-residual-web-styles | 項目 webview 殘留舊網頁樣式：左導航主動狀態使用 text-decoration:underline（套件主動導航係主色/粗體標籤 + 主色軌道欄指示器）；幾個身體/作者區塊 font-size:15px（套件正文 14px）；.returnBtn 20px | Project.jsx（主動導航處理）；typography.css（正文 14px） | resources/web/model_new/css/navigation.css:42; index.css:59 | 移除 .nav-item.active a 上嘅底線（仰賴 font-weight:700 + 主色軌道欄指示器）；將 15px 身體/作者文字標準化到 14px | small | low | done |
| project-webview-legacy-anatomy | 項目標籤保留其原本嘅竹子「模型詳細資料」解剖學，固定 1100px-min 容器，有 890px 內容欄 + 190px 粘性錨點導航疊喺相冊/說明/配件/檔案流，唔係套件嘅類別側欄 + 檔案網格；令牌/半徑/字體已遷移 | Project.jsx（250px 類別側欄 + 項目/版本控制卡 + 「查看歷史」藥丸；內容標題加 SearchField + 已填 「添加檔案」；檔案網格 minmax(180px) 卡）| resources/web/model_new/index.html:20 | 要麼重建 index.html 到 Project.jsx 解剖學（類別側欄加項目 + 版本控制卡、內容標題加 SearchField + 已填添加按鈕、回應式檔案卡網格），要麼記錄此詳細資料頁面版面配置作為有意嘅產品偏差 | large | high | deviation |

---

## 介面：dialogs-flows（葉對話框同埋模態流程）

| 編號 | 舊元件 | 套件參考 | 原生錨點 | 必要改動 | 大小 | 風險 | 狀態 |
|----|----------------|---------|---------------|-----------------|------|------|--------|
| unsavedchanges-shell-and-button-rows | DPIDialog 加 wxCAPTION+wxCLOSE_BOX 庫存標題欄；冇套件標題欄/圖示標題/副標題；add_btn SetCornerRadius(12) 同埋已填按鈕喺 BrandGreen 上使用白色文字而唔係 OnPrimary | containment/Dialog.prompt.md；actions/Button.jsx | UnsavedChangesDialog.cpp:822 | 重新父類別到套件對話框殼（圓形 + 標題欄圖示標題/標題/副標題/關閉 + 頁尾 flex-end）；轉換儲存/放棄/轉移到套件按鈕變數（已填藥丸主色加 OnPrimary 文字、文字/outline 次要）；移除固定 r12；保留差異表喺套件本體 | medium | medium | done |
| calib-dlg-native-radiobox-groupbox-radius3 | 五個校準對話框（PA/溫度/最大容積/VFA/縮回）係 DPIDialog 庫存 chrome，有原生 wxRadioBox、wxStaticBoxSizer 分組盒、純標籤、正方形 TextInputs 同埋 BrandGreen + 白色 + r3 開始按鈕 | containment/Dialog.prompt.md；selection/SegmentedControl + Chip；fields/ValueField；selection/Checkbox；Calibration.jsx | calib_dlg.cpp:40 | 將全部五個重新父類別到套件對話框殼；wxRadioBox → SegmentedControl/Chip 分組；wxStaticBoxSizer → 卡 + SectionHeader；TextInput → ValueField/SelectField（r10、sc-highest、單間距）；打印數字複選框 → 套件複選框；r3 開始 → 已填藥丸按鈕 | large | medium | done |
| thermalpreconditioning-raw-wxdialog-nativebutton | 純 wxDialog（連 DPIDialog 都唔係）加庫存標題欄；視窗圖示透過 create_scaled_bitmap(「thermal_preconditioning_title」)；確定係原生 OS wxButton，只能用 PrimaryContainer/OnPrimaryContainer 樣式；內容冇套件標題欄/圖示標題 | containment/Dialog.prompt.md；actions/Button.jsx | ThermalPreconditioningDialog.cpp:16 | 重建喺套件對話框殼上（圓形 + 標題欄圖示標題 + 標題/副標題）同埋用套件已填/tonal 藥丸按鈕取代原生 wxButton；將本體文字移入套件本體，有類型角色 | medium | low | done |
| sendtoprinter-shell-panels-buttons | DPIDialog 庫存 chrome；儲存面板 bg 白色 + 分隔符 Grey250；wxCB_READONLY ComboBox 選擇器；重新整理 SetCornerRadius(10) + 發送 SetCornerRadius(6)；AnimaIcon 光柵微調工具；冇套件標題欄/打印機卡/Switch 行 | Overlays.jsx（SendDialog）；containment/Dialog.prompt.md；selection/Switch；actions/Button.jsx | SendToPrinter.cpp:214 | 重建到套件 SendDialog：460 寬 28px 殼、打印圖示標題標題欄、打印機卡（sc-highest r16 + 「● 已連接 · 空閒」主色 + expand_more）、AMS 對應行嘅 sc-highest 磚，有 24px 資料樣本、選項行（h40、圖示 + label13 + Switch）、頁尾 Cancel[text lg] + 發送[已填 lg send-icon] 藥丸；用 Surface 角色取代白色/Grey250，用藥丸按鈕取代 r6/10 | large | medium | done |
| publishdialog-legacy-hardcoded-colors | DPIDialog 庫存 chrome；步驟面板 bg #F8F8F8；取消按鈕原始綠色 (27,136,68)/(61,203,115) 加邊框 #00AE42 + r10 + TEXT_LIGHT_GRAY；錯誤標籤 #FF6F00 | containment/Dialog.prompt.md；actions/Button.jsx；Error 角色；containment/ProgressBar.jsx | PublishDialog.cpp:50 | 重新父類別到套件對話框殼；#F8F8F8 → SurfaceContainer 角色；淨化 #00AE42/原始綠色取消到套件文字/outline 藥丸按鈕；將錯誤文字通過 Error 傳遞；移除 TEXT_LIGHT_GRAY 為 OnSurfaceVariant；驗證 ProgressBar 使用主色/sc-highest | medium | medium | done |
| releasenote-family-shell-brandlogo | 發佈備註系列全部係 DPIDialog wxCAPTION+wxCLOSE_BOX；SetBackgroundColour(*wxWHITE) 唔係角色；左品牌標誌透過 create_scaled_bitmap(「BambuStudio」)；卷軸 bg 灰色200；按鈕 SetCornerRadius(12) 加白色文字同埋 *wxWHITE 邊框 | containment/Dialog.prompt.md；actions/Button.jsx | ReleaseNote.cpp:68 | 將系列重新父類別到套件對話框殼（圓形 SurfaceContainer、標題欄圖示標題 + 標題/副標題、頁尾 flex-end）；移除 *wxWHITE bg + 左標誌；卷軸區域用 SurfaceContainer 角色；轉換按鈕到套件藥丸變數，有 OnPrimary 文字 | large | medium | done |
| releasenote-family-partial-migration | ReleaseNote.cpp 內五個同胞類別（SecondaryCheckDialog、PrintErrorDialog、ConfirmBeforeSendDialog、InputIpAddressDialog、ExpandCenterDialog，~2500 行）仍然喺庫存 DPIDialog chrome 上，由 Sonnet 相容性審計重新驗證家族行時發現 | containment/Dialog.prompt.md | ReleaseNote.cpp:458,782,1088,1391,2209 | 將五個同胞重新父類別到 MD3Dialog 殼，保留多按鈕樣式列舉同埋多面板 IP 精靈；需要一次構建驗證傳遞 | large | high | done |
| progressdialog-native-chrome-retained | ProgressDialog 保留原生 chrome；佢嘅雙階段 ctor+Create() 圖案與 MD3Dialog 單次 ctor 不相容；擁有事件迴路/收益/模態禁用邏輯，喺切割/匯出期間使用 | containment/Dialog.prompt.md | Widgets/ProgressDialog.cpp:65-70,138 | 給 MD3Dialog 一個雙階段建構變數或 ProgressDialog 嘅自訂套件殼；行為關鍵、需要一次構建驗證傳遞 | large | high | done |
| updatedialogs-msgupdateconfig-shell | MsgUpdateConfig DPIDialog wxCAPTION 庫存標題欄；SetBackgroundColour(*wxWHITE)；左品牌標誌 create_scaled_bitmap(「BambuStudio」,70)；頂線灰色400；確定使用 *wxWHITE 邊框+文字喺品牌色填充，固定 58x24（冇藥丸） | containment/Dialog.prompt.md；actions/Button.jsx | UpdateDialogs.cpp:98 | 重新父類別到套件對話框殼；移除 *wxWHITE bg 為 SurfaceContainer 角色同埋左標誌為套件標題欄圖示標題；轉換確定/取消到套件已填/文字藥丸按鈕，有 OnPrimary 文字 | medium | medium | done |
| privacyupdate-shell-radius3-buttons | DPIDialog 預設庫存 chrome；SetBackgroundColour(*wxWHITE)；接受/登出按鈕 SetCornerRadius(3) 加白色文字同埋 *wxWHITE 邊框；頂線灰色400 | containment/Dialog.prompt.md；actions/Button.jsx | PrivacyUpdateDialog.cpp:35 | 重新父類別到套件對話框殼（圓形 + 標題欄 + 頁尾）；*wxWHITE bg → SurfaceContainer 角色；轉換 r3 按鈕到套件已填/文字藥丸按鈕，有 OnPrimary 文字 | small | low | done |
| userpresets-searchfield-and-shell | DPIDialog 預設庫存 chrome；搜尋係 TextInput，有 SetCornerRadius(12) + 「im_text_search」光柵圖示（唔係套件 SearchField）；TabCtrl 次要標籤（舊灰色）+ 舊 SwitchButton + 烤 PNG CheckBox | fields/SearchField.jsx；containment/Dialog.prompt.md；navigation/TabBar.jsx；selection/Switch.jsx | UserPresetsDialog.cpp:32 | 重新父類別到套件對話框殼；用套件 SearchField 取代 TextInput 搜尋（r22 藥丸、sc-highest、搜尋字形）；TabCtrl → TabBar 文字角色（主色主動 / OnSurfaceVariant 非主動）；自訂/其他切換 → 套件 Switch 或 SegmentedControl | medium | medium | done |
| stepmesh-shell-buttons | DPIDialog 庫存標題欄；冇套件標題欄/圖示標題；確定/取消 SetCornerRadius(12)（「step_mesh_info」點陣圖係解釋插圖 = 內容） | containment/Dialog.prompt.md；actions/Button.jsx | StepMeshDialog.cpp:72 | 重新父類別到套件對話框殼（圓形 + 標題欄圖示標題 + 標題/副標題 + 頁尾 flex-end）；轉換兩個按鈕到套件藥丸變數（已填確定、文字/outline 取消）；欄位標籤/輸入到套件 ValueField 幾何 | small | low | done |
| textureimport-shell-warning-role | DPIDialog 庫存 chrome（+ wxRESIZE_BORDER）冇套件標題欄/圖示標題；拖放警告標籤使用 ThemeColor::Warning；主要確認文字係白色唔係 OnPrimary；幾個內部按鈕喺 r12/14（主 Skip/Confirm 已經藥丸 r20 h40） | containment/Dialog.prompt.md；Error 角色；Filament.jsx | TextureImportDialog.cpp:1695 | 包裹喺套件對話框殼（標題欄圖示標題 + 標題/副標題、頁尾 OutlineVariant 分隔符）；將拖放警告通過 Error 傳遞；確認文字 → OnPrimary；將其餘 r12/14 內部按鈕標準化為藥丸套件按鈕 | medium | low | done |
| syncams-shell-image-panel-greys | DPIDialog wxCAPTION+wxCLOSE_BOX 庫存 chrome；雙影像比較面板 bgs 硬編碼 wxColour(48,48,48,100)/(246,246,246,100)；StaticBox 次面板；進階選項光柵圖示（advanced_option3/4）；「立即同步」確定 SetCornerRadius(12) + 白色文字；取消 r12 | Overlays.jsx（SendDialog）；containment/Dialog.prompt.md；actions/Button.jsx | SyncAmsInfoDialog.cpp:606 | 重新父類別到套件對話框殼（AMS 風格標題欄圖示標題 + 頁尾）；用 SurfaceContainer 角色取代影像面板灰色；轉換確定/取消到套件藥丸按鈕（已填加 OnPrimary）；遷移進階選項光柵圖示到物料符號 | large | medium | deviation |
| filamentgrouppopup-raster-radio-icons | 墨水組模式選擇彈出式視窗從光柵 PNG（map_mode_on/off/disabled/on_hovered/off_hovered）繪製單選指示器；bg 白色 + 手捲圓形區域；行缺乏套件 SecondaryContainer/主色選擇灰色 | selection/Checkbox.jsx / Chip.jsx（物料符號字形）；containment/Card.jsx；Filament.jsx | FilamentGroupPopup.cpp:88 | 用即時著色物料符號單選字形（已檢查主色 / 未檢查 OnSurfaceVariant）取代五個光柵單選點陣圖；將彈出式視窗放喺 SurfaceContainer 介面上，有 OutlineVariant 邊框 + 高度；給已選行 SecondaryContainer/主色選擇樣式 | medium | low | done |
| helio-release-stock-chrome-radius4 | Helio 陳述/發佈對話框係 DPIDialog wxCAPTION+wxCLOSE_BOX 庫存標題欄，有「了解」按鈕喺 SetCornerRadius(4) + 白色文字；永遠深色 HELIO_* 品牌調色盤被記錄為有意，係免除（只有庫存 chrome + 非藥丸按鈕幾何係範圍內） | containment/Dialog.prompt.md；actions/Button.jsx | HelioReleaseNote.cpp:75 | 採用套件對話框殼（無邊框圓形、標題欄圖示標題 + 標題/副標題、頁尾 flex-end）同埋套件藥丸按鈕，同時保留刻意嘅 Helio 永遠深色品牌介面顏色（做唔做遷移 HELIO_* 調色盤） | medium | low | done |
| raw-wxmessagebox-bypasses-md3 | 44 個原始 wxMessageBox() 呼叫遍佈 19 個 GUI 檔案，渲染完全 OS 原生訊息盒，繞過 MD3 chrome；某些 grep 搜尋結果係已評論，必須排除 | containment/Dialog.prompt.md（MD3 MessageDialog 係預期取代） | PartSkipDialog.cpp:401 | 用 MD3 MessageDialog（一旦 MsgDialog 基礎被改皮）取代每個活躍原始 wxMessageBox()/原生 wxMessageDialog，篩選出已評論嘅出現。之後加入嘅功能又帶返 22 個（工作區面板 15 個、外觀編輯器 6 個、墨水對應交換 1 個），`GUI_App::show_message_box` 亦包住一個；由 2026-09-29 起全部經 `md3_message_box()`（用 wxMessageBox 嘅參數同回傳值，顯示 Material `MessageDialog`），並由 `ui-md3/tests/message-boxes.test.mjs` 把關 | medium | medium | done |

**合併備註（此介面）：**發現 **`msgdialog-base-shell-anatomy`**（相同 `MsgDialog.cpp:29` 基礎殼、相同重建改動）係**合併進 widgets-containment `msgdialog-base-shell-anatomy`**（共用基礎小工具係更具體嘅首頁）。上面嘅葉對話框行保留不同；佢哋使用改皮嘅基礎殼，各自帶上佢哋自己嘅子類別特定工作（`raw-wxmessagebox-bypasses-md3` 依賴該基礎到達）。

---

## 表面：icons-assets（GUI 圖示 / 資產管道：MaterialIcon enum、wxDC 圖示、GL 精靈、ImGui 圖集）

| ID | 舊式元素 | 套件參考 | 原生錨點 | 需要嘅改變 | 大小 | 風險 | 狀態 |
|----|---------|--------|--------|----------|------|------|------|
| expand-materialicon-glyph-enum | MaterialIcon::Glyph enum 得番 28 個 PUA 碼位；缺咗套件規範字形集嘅大部分同埋需要用來放棄常用舊式光柵圖示嘅字形 | icons.html（規範 Material Symbols Outlined 集） | Widgets/MaterialIcon.hpp:40 | 添加套件規範嘅 cmap 認證 PUA 碼位（view_in_ar、layers、cast、devices、folder_open、build、palette、print、tune、account_tree、deployed_code、grid_view、sync、history、restore、file_download、send、thermostat、mode_fan、inventory_2、control_camera、open_with、rotate_right、open_in_full、align_horizontal_left）以及 edit/delete/content_copy/save/undo/redo/warning/error/info/help/lock/lock_open/star/done/straighten/minimize/crop_square/signal_wifi_*/arrow_back/arrow_forward。對照內置嘅 TTF cmap 驗證各個。係下面 wxDC 字形交換嘅前置要求 | 中等 | 低 | 完成 |
| topbar-chrome-raster-to-glyph | 標題欄 AUI 工具列透過 create_scaled_bitmap（topbar_save/undo/redo/publish/min/max/win/close/open/store/line + *_inactive）構建每粒按鈕；顏色已遷移但字形仍然係 Bambu 光柵 | icons.html；App.jsx（46px 標題欄）；readme.md | BBLTopbar.cpp:277 | 透過 MaterialIcon::bitmap(glyph, OnSurface/OnSurfaceVariant) 渲染各個工具點陣圖，禁用狀態透過淡化顏色而非 *_inactive PNG；對應 save→save、undo→undo、redo→redo、calib→tune、publish→publish、min→minimize、max→crop_square、win→filter_none、close→close、open→folder_open（透過顏色表達狀態，從不用 FILL 軸） | 中等 | 中等 | 完成 |
| viewport-main-toolbar-svg-sprites | 主要 3D 編輯器工具列透過 GLToolbar::load_from_svg_files_as_sprites_array 載入原始 Bambu SVG 精靈對（light+dark）（toolbar_open/add_plate/orient/arrange/variable_layer_height/split_*/more/assemble + arrow/seperator） | Prepare.jsx + Overlays.jsx（66px 動作欄 / 字形軌道配 Material Symbols）；icons.html | GLCanvas3D.cpp:7773 | 引入字形→GL 紋理橋接（將 MaterialIcon 字形渲染到 wxBitmap，然後餵到精靈陣列/紋理上載），咁樣工具列項目解析為 Material Symbols 而唔係 _dark PNG/SVG 對；對應 add→folder_open、addplate→grid_view、orient→screen_rotation、arrange→auto_awesome_mosaic、layersediting→layers、splitobjects→call_split、splitvolumes→content_cut、More→more_horiz、assembly→view_in_ar。最大單一工作區槓桿；需要新嘅 GL 渲染路徑 | 大 | 高 | 完成 |
| gizmo-rail-svg-icons | 每個字形工具從 get_icon_filename 返回舊式 toolbar_*.svg（light/dark），作為 GL 精靈渲染（Move/Rotate/Scale/Flatten/Cut/MeshBoolean/FdmSupports/Seam/Text/Svg/ColorPainting/FuzzySkin/Measure/Assembly/Simplify/BrimEars/...）；重置/適應相機色度透過 IMTexture::load_from_svg_file | Overlays.jsx（左邊字形軌道）；icons.html | GLGizmoMove.cpp:121 | 將字形工具圖示路由經同一字形→GL 紋理橋接；對應 Move→open_with、Rotate→rotate_right、Scale→open_in_full、Flatten→align_horizontal_left、Cut→content_cut、MeshBoolean→join_inner、FdmSupports→hardware、Seam→timeline、Text→text_fields、Svg→shape_line、ColorPainting→brush、FuzzySkin→grain、Measure→straighten、Assembly→view_in_ar、Simplify→compress、BrimEars→padding；放棄 _dark 變體（顏色驅動）。依賴 GL 橋接 落地：字形橋接對應每個軌道字形工具並重繪精靈圖集；執行時證據 docs/screenshots/main-window/sidebar-prepare--gizmo-rail.png 同埋旁邊嘅各字形工具裁剪（Mesa 路由，2026-09-06） | 大 | 高 | 完成 |
| statuspanel-monitor-control-icons | 設備監控控制欄幾乎完全由光柵 ScalableBitmaps 構建（monitor_lamp/fan/speed/recording/timelapse/vcamera/camera/prediction/cost/print、filament_load_fold/expand、axis_home、network_wired）；~45 個 ScalableBitmap 成員 | Device.jsx（青色設備配色）；icons.html | StatusPanel.cpp:2088 | 將單色控制/狀態字形轉換為 MaterialIcon（設備配色若果有重點）：lamp→lightbulb、fan→mode_fan、speed→speed、recording→radio_button_checked、timelapse→timelapse、vcamera→videocam、camera→photo_camera、prediction→schedule、cost→payments、print→print、fold/expand→expand_less/more、axis_home→home、network_wired→lan；保持打印機相片/縮略圖作為資產；放棄 _dark 變體 | 大 | 中等 | 完成 |
| monitor-signal-status-icons | 連接性/狀態指示器使用光柵（monitor_signal_strong/middle/weak/no、monitor_arrow、printer_status_idle/busy/offline/lock、printer_in_lan、monitor_upgrade_*）；信號強度冇字形 | icons.html；Device.jsx + Multi.jsx | Monitor.cpp:151；Auxiliary.cpp:815 | 將單色指示器轉換為 MaterialIcon：arrow→chevron_right、status_lock→lock、idle/busy→circle/sync、in_lan→lan；添加 signal_wifi_4_bar / network_wifi_3_bar / _2_bar / signal_wifi_statusbar_null 字形（新 enum 成員）由狀態著色；打印機產品相片保持作為資產 | 中等 | 中等 | 完成 |
| ams-widget-icon-family | AMS/墨水控制項叢集透過 AMSControl/AMSItem/AmsMappingPopup/AMSMaterialsSetting/AMSSetting 由光柵渲染單色親和力（ams_setting/editable/readonly/refresh/arrow、add_filament、delete_filament、ams_drying、filament_hint、hum_popup_close、ams_wheel） | Filament.jsx；icons.html | Widgets/AMSControl.cpp:212；Widgets/AMSItem.cpp:982；AmsMappingPopup.cpp:1801 | 將單色 UI 字形轉換為 MaterialIcon（設備配色）：setting→settings、editable→edit、readonly→lock、refresh→refresh、arrow→chevron_left/right、add_filament→add、delete_filament→delete、drying→dry_cleaning、filament_hint→info、popup_close→close、wheel→sync；保留盤/RFID/墨水插槽圖形作為資產 | 大 | 中等 | 完成 |
| object-outliner-tree-icons | 物件大綱/樹從光柵渲染功能徽章（variable_layer_height、mmu_segmentation、support、fuzzyskin、sinking、dot）+ GUI_ObjectTable lock/check_on/check_off + GUI_ObjectList organize + GUI_Factories cog | Prepare.jsx（物件列表）；icons.html | ObjectDataViewModel.cpp:213 | 透過 MaterialIcon 渲染樹/上下文字形：variable_layer_height→layers、mmu_segmentation→palette、support→hardware、fuzzyskin→grain、sinking→vertical_align_bottom、organize→sort、cog→settings、lock→lock、check_on/off→check_box/check_box_outline_blank、dot→fiber_manual_record；保持墨水/材料樣本單元格作為資料 | 中等 | 中等 | 完成 |
| shared-dialog-action-icons | 跨越對話框/彈出視窗嘅一族無處不在嘅單色動作/狀態字形係作為光柵載入（warning/error/info/question/help、check/completed、refresh、lock/unlock、edit、delete、add、導航箭頭、save、more_info、cross、cut、star、print-time/weight、map_mode、fila_switch）；代表約 140 個不同嘅 create_scaled_bitmap 名稱遍布 58 個檔案 | icons.html；actions/*.prompt.md；readme.md | MsgDialog.cpp:642；SelectMachine.cpp:544；GUI_Factories.cpp:766；FilamentGroupPopup.cpp:90 | 掃過呢啲為 MaterialIcon（字形 + 語義顏色）：warning→warning、error→error、info→info、help→help、check/completed→task_alt、refresh→refresh、lock/unlock→lock/lock_open、edit→edit、delete→delete、add→add、nav→chevron/arrow、save→save、more_info→more_horiz、cross→close、cut→content_cut、star→star、print-time→schedule、print-weight→scale、map_mode→map、fila_switch→swap_horiz。需要先進行 enum 擴展；高計數，每個網站低複雜性 | 大 | 中等 | 完成 |
| imgui-atlas-font-family | ImGui 圖集載入舊式字形族（HarmonyOS_Sans_SC Regular/Bold + NanumGothic）作為預設/粗體；MD3 規範係 Roboto / Roboto Mono | type-scale.html + type-mono.html；readme.md（Roboto 300-700 + Roboto Mono） | ImGuiWrapper.cpp:2744 | 重新針對 AddFontFromFileTTF 呼叫為 Roboto（散文）+ Roboto Mono（數字），保持 CJK 後備合併；對齊基礎大小至 MD3 比例（14 舒適 / 13 緊密） | 中等 | 中等 | 完成 |
| imgui-overlay-icon-glyphs | ImGui 圖層圖示烤入圖集作為來自舊式 SVG 名稱嘅自訂矩形字形（font_icons：PrintIconMarker→'cog'、PrinterIconMarker→'printer'；notification_close）透過 AddCustomRectFontGlyph；唔係 Material Symbols，而且因為圖集從不合併該字面因此唔係字形能力 | icons.html（所有 UI 字形使用 Material Symbols Outlined）；readme.md | ImGuiWrapper.cpp:52 | 將 MaterialSymbolsOutlined.ttf 合併到圖集（AddFontFromFileTTF + PUA 範圍）並用 Material Symbols 碼位替換自訂矩形 SVG 標記（cog→settings、printer→print、notification_close→close）。架構圖集重建；字形族交換後最大 ImGui 槓桿 | 大 | 高 | 完成 |

> [!WARNING]
> **`gizmo-rail-svg-icons` 被標記為 `done` 但唔係。**重新審計於 2026-07-28 對比現時樹。GL 軌道圖集半部確實落地咗，八個重置鍵被遷移到字形橋接（`GLGizmosManager.cpp:163-200、217-239`）；但從 `GLGizmosManager.cpp:241` 開始嘅所有嘢仍然係 `IMTexture::load_from_svg_file` 對舊式 Bambu SVG 配手工編寫嘅 `_dark` 孿生：`toolbar_tooltip{,_hover}`、`fit_camera{,_hover,_dark,_dark_hover}`、`text_B`/`text_T{,_dark}` 同埋 24 個 `align_*`/`distribute_*` 項目。
>
> 當中三個**刻意唔係以呢個行描述嘅方式轉換得到**：
> - `fit_camera` 同埋 `camera_lock` 係複合物，其白色圓形底板係載入軸承；佢哋渲染到無背景 ImGui 視窗超出 3D 場景，所以裸字形會喺淺色模型上無法辨認。
> - `align_*`/`distribute_*` 磁貼喺佢哋嘅美術作品裏編碼軸。呢個字形得一個對齊字形，Z 冇嘢，所以上面提出嘅對應會摺疊十二個不同意義成一個。
> - `toolbar_tooltip` 係 30x22 嘅**鍵盤**提示標記，~13 個呼叫網站大小化佢 `1.8 x 1.3`；字形大約 1:1.17 會拉伸 ~60%。
>
> 關閉呢一列需要新 `MaterialIcon::Glyph` 碼位（`format_bold`、`format_italic`、鍵盤標記）、`GLIconGlyphBridge` 上嘅鍍金字形項目入口、同埋 Z 軸磁貼嘅決定；或保留佢哋作為 MD3 token 化美術作品嘅決定。

## 分波計劃

128 個開放缺口按照指示排序如下：**先係快速勝利、然後係圖示替換、然後係結構解剖、最後先係大型或高風險嘅重組**。喺每個波次內，**平行小組擁有不相交嘅檔案**，所以一個小組可以唔洗擔心阻礙另一個。有幾個好大嘅檔案係被好多缺口共享；呢啲係聲明為**單一擁有者嘅串行軌道**，喺連續嘅波次入面出現（同一個擁有者返嚟推進該檔案）。

### 全域先決條件（喺依賴佢哋嘅各波之前先構建）

- **MaterialIcon 字形列舉** (`expand-materialicon-glyph-enum`, `Widgets/MaterialIcon.hpp`)，門檻係**每個** wxDC 光柵→字形交換（第 3 波，同埋好多設備/準備列嘅字形半部分）。
- **ImGui 字體圖冊** (`imgui-atlas-font-family`, `preview-imgui-mono-font`,
  `imgui-overlay-icon-glyphs`，全部喺 `ImGuiWrapper.cpp`)，門檻係所有 ImGui 覆蓋圖字形/單色工作
  （預覽時間表/統計/Z 晶片/選項晶片、單層圖示）。
- **字形→GL-紋理橋接** (由 `viewport-main-toolbar-svg-sprites`,
  `GLToolbar.cpp`/`GLCanvas3D.cpp` 引入)，門檻係所有 GL 工具欄/gizmo 圖示工作
  (`gizmo-rail-svg-icons`，同埋兩個 GL 工具欄重新皮膚嘅圖示半部分)。
- **共享套件小部件**，繪製嘅 Checkbox/Radio、Switch、DropDown、欄位幾何體
  （TextInput/ComboBox/SpinInput）、SearchField、Slider、SegmentedControl、IconButton 同埋
  SectionHeader（全部喺 `widgets-core`/`widgets-containment`），門檻係佢哋嘅表面消費者
  （準備流程卡、設備打印選項、設定外觀、對話殼）。呢啲小部件
  係**大型**，所以按照尺寸排序佢哋喺第 6 波 登陸；第 5 波–7 入面每個參考套件欄位/開關/分段控制嘅中型/大型卡都帶有
  一條依賴邊到相應嘅第 6 波 小部件，必須喺佢之後排程。

### 爭用嘅單一擁有者串行軌道

`Plater.cpp`、`MainFrame.cpp`、`StatusPanel.cpp`、`BaseRenderer.cpp`、`IMSlider.cpp`、
`ImGuiWrapper.cpp`、`GLCanvas3D.cpp`/`GLToolbar.cpp`、`Preferences.cpp`、`Tab.cpp`、`BBLTopbar.cpp`、
`Notebook.cpp`/`Button.cpp`、`MsgDialog.cpp` 同埋 `AMSControl.cpp` 係各自被多個缺口編輯。
指派一個擁有者俾每個檔案；個擁有者按波次順序推進佢嘅缺口（快速 → 圖示 → 解剖 →
大型）。同一個波次入面冇兩個平行小組可以同時持有呢啲檔案嘅其中一個。

---

### 第 1 波：快速勝利（小型/低風險、純粹顏色/幾何、冇字形或殼依賴）

完全平行；下面嘅每個小組擁有一個不相交嘅檔案。

| 小組（擁有） | 缺口 ID |
|---|---|
| `Widgets/StaticGroup.cpp` | staticgroup-card-white-bg |
| `Widgets/StaticLine.cpp` | staticline-divider-color |
| `Widgets/Scrollbar.cpp` | scrollbar-chrome-white-square |
| `Widgets/ProgressBar.*` | progressbar-geometry-pill-height |
| `Notebook.cpp` | tabbar-hover-shape-rounded, tabbar-font-and-padding-metrics |
| `MainFrame.cpp` | frame-page-panels-legacy-white, estimate-block-single-font-no-length |
| `Preferences.cpp` | prefs-white-backgrounds-no-surface-layering, prefs-bottom-buttons-legacy-geometry |
| `Tab.cpp` | tab-editor-white-panel-backgrounds |
| `BaseRenderer.cpp` | preview-color-scheme-sectionheader, preview-legend-swatch-geometry, preview-slicing-result-title, preview-gcode-window-syntax-colors, preview-recommendation-overlay-tints, preview-viewport-status-pill |
| `IMSlider.cpp` | preview-slider-tooltip-legacy |
| `StatusPanel.cpp` | device-progress-title-strip |
| `MediaFilePanel.cpp` | device-storage-tab-white-panel |
| `SelectMachinePop.cpp` | device-selectmachine-popup-legacy-literals |
| `WebUserLoginDialog.cpp` | login-dialog-legacy-native-parts |
| `resources/web/model_new/css/*` | project-webview-residual-web-styles |
| `ParamsPanel.cpp` | tipsdialog-button-brandgreen-literal-radius |

*（預覽 `preview-color-scheme-sectionheader` 內聯樣式 ImGui 文字，11/600/uppercase，唔需要
共享嘅 SectionHeader 小部件。）*

### 第 2 波：基礎圖示 / 類型啟用器

兩個不相交嘅小組，平行。第 3 波–4 下游嘅一切都依賴於呢啲。

| 小組（擁有） | 缺口 ID |
|---|---|
| `Widgets/MaterialIcon.hpp` | expand-materialicon-glyph-enum |
| `ImGuiWrapper.cpp`（串行內：字體族 → 單色 → Material Symbols 合併） | imgui-atlas-font-family, preview-imgui-mono-font, imgui-overlay-icon-glyphs |

### 第 3 波：wxDC 光柵→字形圖示替換（依賴第 2 波 列舉）

每個小組擁有不相交嘅檔案。呢啲通過現有嘅 wxDC 路徑交換 `ScalableBitmap`/`create_scaled_bitmap` 為
`MaterialIcon` 字形。

| 小組（擁有） | 缺口 ID |
|---|---|
| `ObjectDataViewModel.cpp`（+ GUI_ObjectTable/List、GUI_Factories） | object-outliner-tree-icons |
| `Monitor.cpp` / `Auxiliary.cpp` | monitor-signal-status-icons |
| `AMSControl.cpp` / `AMSItem.cpp` / `AmsMappingPopup.cpp` | ams-widget-icon-family |
| `WebViewDialog.cpp` | home-online-toolbar-raster-icons |
| `ModelMall.cpp` | modelmall-store-legacy-toolbar-chrome |
| `FilamentGroupPopup.cpp` | filamentgrouppopup-raster-radio-icons |
| `BBLTopbar.cpp`（圖示切片） | topbar-chrome-raster-to-glyph, titlebar-window-controls-raster |
| `MainFrame.cpp`（圖示切片） | plate-chip-missing-grid-view-glyph, add-plate-plus-text-vs-glyph, action-bar-residual-raster-chrome-helio-splitline |
| `Plater.cpp`（圖示切片） | add-filament-footer-raster-icon |
| `StatusPanel.cpp`（圖示切片） | device-camera-hud-raster-status-glyphs, device-progress-metadata-raster-icons, statuspanel-monitor-control-icons |
| 葉子對話圖示掃描（MsgDialog/SelectMachine/GUI_Factories/…，同 MsgDialog 殼擁有者協調） | shared-dialog-action-icons |

備註：`shared-dialog-action-icons` 觸及 `MsgDialog.cpp` 同埋 `SelectMachine.cpp`，對話殼工作（第 7 波）都會觸及，同一個擁有者應該按序做圖示交換先過殼重建，或者兩者一齊做。`BBLTopbar.cpp` 嘅圖示切片係同一個擁有者，後來會做標題欄重組（第 7 波）；`titlebar-window-controls-raster` 字形同埋更廣泛嘅 `topbar-chrome-raster-to-glyph` 掃描喺最小化/最大化/關閉上重疊，必須由個擁有者協調。

### 第 4 波：ImGui / GL 覆蓋圖示 & 字體應用（依賴第 2 波 圖冊 / GL 橋接）

| 小組（擁有） | 缺口 ID |
|---|---|
| `BaseRenderer.cpp`（預覽覆蓋） | preview-options-icon-chips, preview-viewmode-chip-layout, preview-statistics-typography, preview-section-titles-legend |
| `IMSlider.cpp` | preview-one-layer-raster-icons, preview-vertical-slider-z-chip |

`preview-statistics-typography` 同埋 `preview-vertical-slider-z-chip` 依賴第 2 波 單色字體；
`preview-options-icon-chips` 同埋 `preview-one-layer-raster-icons` 依賴第 2 波 Material
Symbols 圖冊合併。GL 工具欄/gizmo 圖示橋接（`viewport-main-toolbar-svg-sprites`、
`gizmo-rail-svg-icons`）喺第 7 波 同埋佢哋嘅解剖重組一齊登陸。

### 第 5 波：結構解剖（中型小部件重新皮膚 + 中型表面卡）

平行小組擁有不相交嘅檔案。消費套件欄位/開關/分段控制嘅列標記為
第 6 波 依賴。

| 小組（擁有） | 缺口 ID | 依賴於 |
|---|---|---|
| `Widgets/StaticBox.*` | staticbox-card-no-hover-border |  |
| `Widgets/TabCtrl.cpp` | tabctrl-secondary-tab-indicator-text | MaterialIcon 列舉 |
| `Tabbook.cpp` / `TabButton.cpp` | tabbook-selected-tab-anatomy | MaterialIcon 列舉 |
| `Widgets/ProgressDialog.*` | progressdialog-shell-white-bg |  |
| `HMSPanel.cpp` | device-hms-tab-unmigrated |  |
| `MediaPlayCtrl.cpp` | device-mediaplayctrl-bar-anatomy-and-raster | MaterialIcon 列舉 |
| `MainFrame.cpp`（解剖切片） | slice-print-buttons-brandgreen-literals-wrong-anatomy |  |
| `StatusPanel.cpp`（解剖切片） | device-pause-stop-md3-buttons, device-section-headers-missing-glyphs | SectionHeader 樣式；MaterialIcon 列舉 |
| `Plater.cpp`（卡切片） | printer-identity-card-combobox-anatomy, bed-card-84px-vs-selectfield, section-headers-legacy-titlebar-raster-icons, object-manipulation-sidebar-card-absent, filament-subtitle-row-legacy-raster-buttons, sync-info-button-not-in-kit-printer-section, filament-title-purge-flush-aux-buttons | SectionHeader；SelectField/ValueField（第 6 波） |
| `GLCanvas3D.cpp`（視口覆蓋切片） | viewport-overlays-zoom-cluster-statpill-axis | IconButton |
| `Preferences.cpp`（欄位切片） | prefs-value-select-field-chrome, prefs-search-field-missing | ValueField/SelectField/SearchField（第 6 波） |
| `Tab.cpp`（欄位/工具欄切片） | tab-editor-search-field-geometry-glyph, tab-editor-toolbar-raster-iconbuttons | SearchField；IconButton |
| `BBLTopbar.cpp`（晶片切片） | titlebar-brand-tile-png, titlebar-missing-history-chip, titlebar-project-chip, titlebar-appearance-palette-button | MaterialIcon 列舉 |
| 對話殼，中型葉子對話（各自其碩檔案，全部平行） | unsavedchanges-shell-and-button-rows, thermalpreconditioning-raw-wxdialog-nativebutton, publishdialog-legacy-hardcoded-colors, updatedialogs-msgupdateconfig-shell, userpresets-searchfield-and-shell, textureimport-shell-warning-role, helio-release-stock-chrome-radius4 | MsgDialog 套件殼（第 6 波/7） |

備註：中型葉子對話重新掛接到套件對話殼，所以雖然好多被標記為中型佢哋都唔可以喺共享殼存在之前完成，排程喺第 6 波/7 嘅殼登陸後，或者先構建一個薄殼幫手。

### 第 6 波：基礎共享小部件構建（大型小部件，解鎖消費者）

喺大型工作中優先構建呢啲；上面參考嘅第 5 波/7 消費者依賴於佢哋。
平行小組擁有不相交嘅檔案。

| 小組（擁有） | 缺口 ID | 風險 |
|---|---|---|
| `Widgets/CheckBox.cpp` | checkbox-drawn-glyph | 中等 |
| `Widgets/RadioBox.cpp` | radiobox-drawn-glyph | 低 |
| `Widgets/SwitchButton.cpp`（串行：晶片 → 分段 → 開關） | customtogglebutton-chip-anatomy, switchboard-segmented-tokens, switchbutton-md3-switch | 中等 |
| `Widgets/TextInput.cpp` | textinput-md3-field-geometry | 中等 |
| `Widgets/ComboBox.cpp` | combobox-selectfield | 中等 |
| `Widgets/SpinInput.cpp` | spininput-valuefield | 中等 |
| `Widgets/DropDown.cpp` | dropdown-floating-surface | 中等 |
| 新 `Widgets/SearchField.*` | no-shared-md3-searchfield | 中等 |
| 新 `Widgets/Slider.*` | no-shared-md3-slider | 低 |
| `Widgets/Button.*`（IconButton + 字形渲染路徑） | button-iconbutton-and-glyph-icons | 中等 |
| `Widgets/Label.hpp`（SectionHeader 樣式） | sectionheader-style-missing | 低 |

`ComboBox`/`SpinInput` 依賴于 `TextInput` 欄位幾何體優先登陸（同一個欄位族，
不同檔案）。`Button` 字形渲染路徑解鎖 `tabbar-icons-raster-no-fill`（第 7 波）。

### 第 7 波：大型 / 高風險重組（最後；風險已述）

平行小組擁有不相交嘅檔案。每個條目述明其風險同埋依賴。

| 小組（擁有） | 缺口 ID | 風險 & 關鍵依賴 |
|---|---|---|
| `MsgDialog.cpp`/`.hpp`（串行） | msgdialog-base-shell-anatomy, msgdialog-footer-button-geometry, dpidialog-subclass-topline-chrome | **高**，重建基殼層疊到約 12 個子類；迴歸表面係每個訊息/確認對話。優先登陸基礎；頁腳 + 子類頂線跟隨。解鎖所有第 5 波 中型葉子對話同埋 `raw-wxmessagebox-bypasses-md3`。 |
| 葉子對話殼（各自其碩檔案，平行） | calib-dlg-native-radiobox-groupbox-radius3, sendtoprinter-shell-panels-buttons, releasenote-family-shell-brandlogo, syncams-shell-image-panel-greys, privacyupdate-shell-radius3-buttons, stepmesh-shell-buttons | 中等/低，依賴於 MsgDialog 套件殼 + 第 6 波 嘅套件 Button/SegmentedControl/ValueField/Switch/Checkbox。 |
| `PartSkipDialog.cpp` + 18 個 GUI 檔案（wxMessageBox 掃描） | raw-wxmessagebox-bypasses-md3 | 中等，依賴於 MsgDialog 基殼；排除註解出嘅呼叫網站。 |
| `GLCanvas3D.cpp` / `GLToolbar.cpp`（串行：構建字形→GL-紋理橋接、然後重新皮膚） | viewport-main-toolbar-svg-sprites, top-scene-toolbar-opengl-not-md3, gizmo-rail-opengl-not-md3 | **高**，OpenGL 渲染路徑變化，GLToolbar 冇 MD3 先例；新固定軌道/居中藥丸解剖 + 紋理管道。橋接係第 4 波/7 先決條件針對 GL 圖示。 |
| `GLGizmo*.cpp` / `GLGizmosManager.cpp` | gizmo-rail-svg-icons | **高**，每個 gizmo 圖示字形經由 GL 橋接；同 gizmo 軌道解剖上共同排程（同一個視覺表面、相鄰檔案）。 |
| `StatusPanel.cpp`（串行） + `AxisCtrlButton.cpp` + `AMSControl.cpp` | device-control-title-strip-and-action-buttons, device-temperature-rows-raster-and-anatomy, device-print-options-segmented-sliders-switch, device-move-xy-axis-dial-vs-grid, device-move-z-extruder-raster-icons, device-ams-card-legacy-widget | **高**，StatusPanel 係最爭用嘅檔案；移動轉盤（AxisCtrlButton）同埋 AMS 卡（AMSControl）係分開嘅舊版小部件驅動實況設備流。打印選項依賴於第 6 波 SegmentedControl + Switch + Slider。 |
| `MultiMachineManagerPage.cpp` / `MultiMachinePage.cpp` | device-multi-farm-list-vs-card-grid | **高**，列表→回應卡網格重組。 |
| `Plater.cpp`（大型重建切片） | process-legacy-paramspanel-tree, filament-rows-preset-combobox-not-inforow, objects-legacy-searchctrl-dataviewctrl | **高**，用套件卡替換 ParamsPanel 樹、墨水預設合併同埋 ObjectList DataViewCtrl；依賴於第 6 波 欄位/分段/開關小部件 + SearchField。 |
| `Notebook.cpp` / `Button.cpp`（標籤字形渲染） | tabbar-icons-raster-no-fill | 中等，依賴於第 6 波 Button 字形渲染路徑（FILL 0/1 切換）。 |
| `BBLTopbar.cpp`（控制移除） | titlebar-remove-nonkit-controls | **高**，從標題欄移除儲存/撤銷/重做/校準/發佈並重新定位佢哋嘅功能；產品行為變化、同第 3 波/5 BBLTopbar 切片協調（同一個擁有者）。 |
| `NotificationManager.cpp` | notification-snackbar-inverse-roles-placement | **高**，重新著色為反向角色（已完成）；位置記錄為刻意偏差：右下角堆疊（產品授權）、唔係套件嘅下方居中列。 |
| `IMSlider.cpp`（時間表） | preview-timeline-bar-missing | **高**，新不透明 58px 傳輸欄 + 播放動作；依賴於第 2 波 ImGui 單色字體 + Material Symbols 圖冊。 |
| `Preferences.cpp`（大型切片） | prefs-nav-rail-replaces-horizontal-tabbar, prefs-appearance-section-missing, prefs-row-builders-legacy-colors-fonts-layout | 中等/高，NavRail 替換水平標籤欄；外觀部分需要新 Density/Accent 基礎設施 + 本地 SegmentedControl（第 6 波）。 |
| `Tab.cpp`（分類導航） | tab-editor-tabctrl-tree-and-raster-icons | 中等，TabCtrl 樹 → MD3 NavItem 藥丸同埋字形。 |
| `resources/web/model_new/index.html` | project-webview-legacy-anatomy | **高**，完整 webview 重新佈局到套件項目解剖、或者記錄為刻意產品偏差。 |

---

## 涵蓋範圍

每個審計員嘅涵蓋範圍備註逐字再現如下，所以未來嘅會話知道確切係邊啲被
涵蓋，最關鍵係邊啲**冇被**審計或係被刻意預留未標記。

### widgets-core

```text
Audited the shared Widgets library (part 1) against the vendored kit. Read current source in full for: Button.cpp/.hpp, CheckBox.cpp/.hpp, RadioBox.cpp, SwitchButton.cpp/.hpp (SwitchButton, SwitchBoard, MultiSwitchButton, CustomToggleButton), TextInput.cpp/.hpp, ComboBox.cpp, SpinInput.cpp, DropDown.cpp, StaticBox.cpp/.hpp, MaterialIcon.hpp, MD3Tokens.hpp. Kit specs consulted: components/actions/{Button,IconButton}.prompt.md, components/selection/{Checkbox,Switch,SegmentedControl,Slider,Chip}.prompt.md, components/fields/{SearchField,SelectField,ValueField}.prompt.md. Re-verified all digest anchors against live code (they had shifted; corrected). CONFIRMED already-migrated (NOT flagged): Button MD3 variant API (Button.cpp:170-309), MultiSwitchButton SegmentedControl tokens (SwitchButton.cpp:881-908), CustomToggleButton semantic colors (SwitchButton.hpp:116-117), TextInput ErrorContainer error state (TextInput.cpp:429), RichTooltipPopup/ExpandButtonHolder Inverse/Surface roles. NOT covered here (belong to other surfaces): Slider/SearchField instances that live in dialogs/chrome (Field.cpp, StepMeshDialog, TextureImportDialog, Plater, SelectMachinePop, Tab, Search, UserPresetsDialog) are noted as gaps only where they represent a missing shared-widget contract. No shared Slider or SearchField widget exists in Widgets/ at all. StaticBox base default radius=compact(12) with no interactive hover-border promotion is a containment-surface concern, not re-flagged here since in-scope fields override radius themselves. Read-only; no edits/builds.
```

### widgets-containment (part 2): dialogs, cards/panels, badges/tags, progress, snackbar/notifications, section headers, scrollbars, tab bars, tooltips

```text
Read-only audit of the shared containment/navigation widget library against the vendored kit (components/containment/*, components/navigation/TabBar*/TabBar.prompt.md, guidelines/shape-elevation.html, layout-metrics.html, density.html). Every native anchor below was re-read in current code (branch tip), not trusted from the prior digest. AUDITED FILES: MsgDialog.{hpp,cpp} (base + ErrorDialog/WarningDialog/MessageDialog/RichMessageDialog/InfoDialog/DownloadDialog/PostProcessScriptDialog/FilamentWarningDialog + DPIDialog subclasses DeleteConfirmDialog/Newer3mfVersionDialog/NetworkErrorDialog), Widgets/StaticBox.{hpp,cpp}, Widgets/StaticGroup.cpp, Widgets/StaticLine.cpp, Widgets/ProgressBar.{hpp,cpp}, Widgets/ProgressDialog.{hpp,cpp}, NotificationManager.cpp (ImGui toast), Widgets/SwitchButton.cpp RichTooltipPopup, Notebook.cpp (ButtonsListCtrl workspace TabBar), Tabbook.cpp + TabButton.cpp, Widgets/TabCtrl.cpp, Widgets/Scrollbar.cpp, Widgets/Label.hpp. ALREADY CONFORMANT (not flagged): the 9-workspace Notebook tab bar's colors/geometry (Surface bg, OutlineVariant divider, 3px rounded Primary indicator via ColorScheme::Brand, Primary/OnSurfaceVariant text, SurfaceContainerLow hover) match TabBar.jsx; RichTooltipPopup already uses InverseSurface/InverseOn per kit; ProgressBar track/fill colors are already semantic(SurfaceContainerHighest/Primary); the DPIDialog subclasses and MsgDialog have had their COLORS migrated to semantic roles — the residual gaps are anatomy/geometry/raster-icon, not color. NOTE: there is NO shared Badge/Tag/Chip widget in the native library (kit has Badge.jsx) — tags are hand-rolled inside consumer dialogs (SelectMachine, filament/AMS dialogs), which belong to other screen surfaces and were not swept here. Large consumer dialogs (SelectMachine 335KB, CreatePresetsDialog, AmsMappingPopup) are out of this shared-library scope.
```

### chrome-nav

```text
Audited every chrome-nav surface by reading current code (read-only, no build): TitleBar (src/slic3r/GUI/BBLTopbar.cpp full + macOS panel MainFrame.cpp:268-277); TabBar (Notebook.cpp full, Widgets/Button.cpp render/icon path 375-460, tab construction MainFrame.cpp:1024-1684); GizmoRail (GLCanvas3D.cpp gizmo-toolbar init 7796-7883 + 12403, GLToolbar.cpp/GLGizmosManager.cpp render — grep-confirmed ZERO MD3 refs); Settings NavItem nav (Preferences.cpp PreferenceTabbar 1258-1389). Re-verified intel anchors against moved code. CONFIRMED ALREADY DONE (not flagged): TabBar bar bg=Surface + transparent tabs + sc-low hover + 3px brand indicator (Notebook.cpp:84-115,135-148); TitleBar close-button destructive Error hover (BBLTopbar.cpp:172-176); 1x22 OutlineVariant DrawSeparator override (:94-105); macOS panel_topbar migrated to SurfaceContainerLow (MainFrame.cpp:273, was ThemeColor::Grey250); menu buttons File/Edit/View/Objects/Help on MD3 roles + Medium weight; MD3::Metrics chrome heights 46/52 pinned. NOT FLAGGED — no persistent editor status bar exists (only a commented-out m_statusbar ref at MainFrame.cpp:2996) and the kit App.jsx shell defines no status bar, so there is nothing to migrate there. Filament data colors / axis colors treated as data, never flagged. Could not exercise the running app (read-only mandate); GL gizmo-rail render path assessed statically.
```

### prepare

```text
Audited the full Prepare (3D Editor) workspace against ui_kits/bambu-studio/Prepare.jsx and native-prepare.md, re-verifying every anchor in the current tree (code has moved since the digest). Covered: right sidebar shell + all five kit sections (Printer, Filament, Process, Objects, Object manipulation), the 66px bottom action bar (plate chip, add-plate, estimate, Slice/Print, Helio/expand chrome), and the OpenGL viewport chrome (left gizmo rail, top scene toolbar, viewport overlays). Confirmed the token layer (MD3Tokens.hpp) is now complete (Elevation, Viewport axis, TypeStyle, pill_radius all present) and that the bottom-bar background was reconciled to SurfaceContainerLow (kit --md-sc-low). Confirmed the ImGui gizmo panels (GizmoObjectManipulation, GLGizmoText, etc.) and ImGuiWrapper ARE substantially MD3-migrated (130+257 MD3 refs) — so ImGui window chrome is NOT flagged here except where the kit specifies a sidebar card instead of an overlay. NOT covered / cannot verify statically: runtime pixel spacing/gaps of the sidebar sections (values read from source, not rendered), and the exact visual of GL texture-atlas icons (confirmed render path is legacy, not per-glyph). ScalableButton confirmed to be a wxButton over ScalableBitmap (raster/SVG-baked-to-bitmap), so all "ScalableButton icon" items are legacy raster icons, not Material Symbols glyphs. GLToolbar.cpp and GLGizmosManager.cpp have zero MD3 references (verified).
```

### preview

```text
Audited the entire G-code Preview overlay against ui_kits/bambu-studio/Preview.jsx and the component specs (Chip.jsx, SectionHeader.jsx, IconButton.jsx). Verified every anchor by reading current code: BaseRenderer.cpp render_legend (1452-2887), render_all_plates_stats (785-1172), render_slider (1442-1450), estimate/statistics card (2713-2868), filament-grouping recommendation (2905-3168), GCodeWindow (3551-3730), Marker HUD (3799+); IMSlider.cpp render (1057-1131), horizontal_slider (470-549), vertical_slider labels (837-1055), draw_ticks/tooltip; ImGuiWrapper.cpp style stacks (2449-2621) and MD3Tokens.hpp. CONFIRMED ALREADY MIGRATED (not flagged): view-mode chip color tokens (Primary solid fill + OnPrimary text + h/2 radius + height 30 + transparent unselected + Outline border, BaseRenderer 1876-1914); statistics card surface (sc-highest fill + 16 radius + outline-variant hairline, 2862-2866); legend dock WindowBg=SurfaceContainerLow (1481); Marker HUD text tokenized (3821-3822); base ImGuiWrapper toolbar/menu/window stacks fully token-resolved, no green literals (2449-2559); recommendation dashed line + orange delta + text now use OutlineVariant/ThemeColor::Warning tokens (2974,3113,3136); IMSlider tick colors tokenized to Outline/PrimaryContainer (683-684,779). Extrusion-role legend swatch colors, tool-change band colors and custom-gcode tick icon colors treated as DATA (not flagged). render_all_plates_stats is an auxiliary popup not in the kit and already MD3-styled; not flagged. Could not run/build (read-only) so styling was verified structurally in source, not visually.
```

### device

```text
Audited the full Device workspace against ui_kits/bambu-studio/Device.jsx + Multi.jsx, re-verifying every anchor in the current code (StatusPanel.cpp is now 6953 lines; anchors moved from the digest). Covered: StatusPanel StatusBasePanel + PrintingTaskPanel (camera/progress/temp/print-options/move/AMS cards), the new CameraHUD (treated as conformant baseline — audited the raster status indicators hosted inside it), MediaPlayCtrl bar, Monitor tabbook sub-pages (Storage=MediaFilePanel, Firmware=UpgradePanel, HMS=HMSPanel), AMSControl, and the multi-machine + SelectMachine send-picker pages. The MD3 color/token layer is largely applied everywhere (device_* helpers, teal Device scheme, teal progress fill, mono-28 percent) — remaining gaps are almost entirely (a) legacy raster monitor_* PNG control icons where the kit specifies Material Symbols glyphs, (b) legacy widget anatomy (TempInput composites, ImageSwitchButton/FanSwitchButton, circular AxisCtrlButton dial, AMSControl, list-vs-grid device farm, title strips), and (c) a few unmigrated secondary panels (HMSPanel, SelectMachinePop). NOT flagged (data, per mandate): filament/AMS swatch colors and #CECECE/#EEEEEE default-filament placeholders in SelectMachine.cpp / SendMultiMachinePage.cpp, and G-code/paint palettes. Did NOT deep-dive UpgradePanel firmware-version card layout (40 MD3 refs, 0 raw literals — reads conformant) or AMSControl's internal sub-widgets beyond confirming the palette is 100% legacy AMS_CONTROL_*. Read-only; no build run.
```

### settings-params

```text
Audited the full settings/parameters surface against the vendored MD3 kit (ui-md3/design-system: Settings.jsx, components/fields/{ValueField,SelectField,SearchField}, components/navigation/NavItem, and intel native-settings-filament.md). Files read in full or grepped exhaustively: Preferences.cpp (2254 lines — PreferenceTabbar, create()/constructor, all create_item_* row builders, create_bottom_buttons, general tab), Tab.cpp create_preset_tab (top panel, toolbar buttons, search StaticBox, TabCtrl tree, add_scaled_button/add_options_page), ParamsPanel.cpp (ParamsPanel largely MD3-migrated; TipsDialog remnant), ParamsDialog.cpp (clean host shell, no own chrome), Field.cpp / OG_CustomCtrl.cpp / Search.cpp / PresetComboBoxes.cpp (spot-checked — text colors mostly already StateColor::semantic(MD3::Role)). Intel anchors re-verified against current code; several Tab.cpp raw-hex StateColor literals cited in the intel have since been migrated to semantic roles (Tab.cpp:1248/1254/4756), and ParamsPanel/Search/Field/OG_CustomCtrl are now on MD3 roles, so those are NOT re-flagged. The heavy legacy remains concentrated in Preferences.cpp (~5% MD3) plus the Tab.cpp preset-editor chrome (white panels, TabCtrl tree, raster icons, non-pill search). Could not run/build the app (read-only mandate); all gaps verified by reading current source. Data colors (filament swatch clr_picker in PresetComboBoxes) intentionally left unflagged.
```

### home-project

```text
Audited the Home + Project surfaces end to end against the kit. Verified: (1) Home webview CSS — resources/web/homepage3/css/* is fully tokenized (0 hardcoded hex outside the :root token defs in common.css/dark.css; dark path via globalapi.js SwitchDarkMode) → reference-complete, no gap. (2) Project webview CSS — resources/web/model_new/{index,css/*}.css is now token-migrated (index.css, navigation, gallery, editor, accessory_dropdown, tool, dark, black all use var(--md-*); dark override loads via globalapi.js './css/dark.css'; only a photo-counter scrim rgba(0,0,0,.45)+#fff remains, which is correct over an image). (3) ModelPreviewDialog (the NEW MakerWorld pre-import preview, invoked at Plater.cpp:20245) is MD3-conformant — roles/Label/pill buttons; its filled button uses ThemeColor::BrandGreen/White which correctly dark-remap via gDarkColors (StateColor.cpp:9-11,20), so NOT flagged. (4) Project.cpp is a pure webview host with no native color/bitmap literals — clean. Remaining gaps are in the native chrome AROUND the webviews (WebViewDialog online toolbar, ModelMall store window, ZUserLogin login dialog) plus residual Project web-style/anatomy items. Could NOT visually verify rendering (strictly read-only, no build); all anchors re-read against current code. The remote sign-in page shown inside ZUserLogin is server-hosted and out of local control. Cross-cutting raster→Material Symbols icon migration (MaterialIcon helper now exists) is noted where it touches this surface.
```

### dialogs-flows

```text
Audited every leaf dialog/flow in the task list against the kit Dialog spec (components/containment/Dialog.prompt.md + Dialog.jsx) and the Overlays.jsx screen references (SendDialog/AddFilamentDialog/ExportDialog/HistoryDrawer), cross-checking Calibration.jsx/Filament.jsx and the actions/selection specs. Read the current source of: MsgDialog(.cpp/.hpp) family, UnsavedChangesDialog, calib_dlg (5 dialogs), ThermalPreconditioningDialog, SendToPrinter, PublishDialog, ReleaseNote family, UpdateDialogs (MsgUpdateConfig), PrivacyUpdateDialog, UserPresetsDialog, StepMeshDialog, TextureImportDialog, SyncAmsInfoDialog, FilamentGroupPopup, HelioReleaseNote; plus a repo-wide sweep for raw wxMessageBox. DOMINANT FINDING: colors are largely migrated (semantic()/ThemeColor already resolve to MD3 hex, and AMS_CONTROL_BRAND_COLOUR = ThemeColor::BrandGreen = #146c2e is correct), but ANATOMY is uniformly legacy — every dialog uses stock wx window chrome (native OS title bar via wxCAPTION/wxCLOSE_BOX or wxDEFAULT_DIALOG_STYLE) instead of the kit's borderless 28px-radius rounded surface with a custom header (44x44 PrimaryContainer icon tile + 18/600 title + 12.5 subtitle + circular close) and a flex-end footer; buttons use fixed SetCornerRadius(3/4/6/10/12) + fixed 58/76/90x24 sizes and hand-rolled StateColor blocks instead of the kit pill Button variants (radius=height/2, h36/42/44). Filament swatches / AMS mapping tile colors are data and were NOT flagged. MultiPrintJob.cpp is header-only (no chrome). HelioReleaseNote's HELIO_* dark palette is documented as an intentional always-dark partner-brand/data surface and is treated as exempt (only its stock chrome/button geometry flagged). Not separately opened: SelectMachine.cpp (the large send-to-cloud dialog — likely a different surface), CalibrationWizard*/CalibrationPanel (panel-based flow beyond the named calib_dlg), and HelioHistoryDialog internals (only its wxMessageBox usage counted).
```

### icons-assets

```text
Audited the entire GUI icon/asset pipeline in src/slic3r/GUI (worktree bambu-studio-ui-migration-38d0ef) and the vendored kit. Quantified legacy raster/SVG icon usage: create_scaled_bitmap = 321 call sites / 140 distinct icon names / 58 files; ScalableBitmap(parent,"name",px) = 906 sites / 268 distinct names / 128 files; `new ScalableButton(...,"name")` = 78 sites; resources/images ships 1090 .svg + 220 .png/.PNG + 3 .ico/.icns. Verified the MD3 glyph helper (Widgets/MaterialIcon.hpp/.cpp: 28-glyph PUA enum, wxDC DrawText + wxBitmap render paths) and its ADOPTION so far = only 5 files (CameraHUD.cpp/.hpp, AxisCtrlButton.cpp, Label.cpp registration) — the migration lever is almost entirely unused. Verified the two OTHER icon pipelines the kit also governs: (1) viewport GL toolbars/gizmos load `toolbar_*.svg` sprites via GLToolbar::load_from_svg_files_as_sprites_array / GLGizmoBase::get_icon_filename / IMTexture::load_from_svg_file (GLCanvas3D.cpp:7773 _switch_toolbars_icon_filename, GLGizmoMove.cpp:121, GLGizmosManager.cpp); (2) ImGui overlay atlas (ImGuiWrapper.cpp:2744 loads HarmonyOS_Sans_SC/NanumGothic — NOT Roboto; ImGuiWrapper.cpp:52 font_icons map bakes SVG icons as custom-rect glyphs, ImGuiWrapper.cpp:2786 AddCustomRectFontGlyph — no Material Symbols in the atlas). Cross-checked kit canon: guidelines/icons.html (canonical ~33-glyph Material Symbols Outlined set) + readme.md (Icons/Type/Layout foundations) + ui_kits/bambu-studio/*.jsx. Kit glyph set already covers move/rotate/scale/etc via open_with, rotate_right, open_in_full, view_in_ar, control_camera, deployed_code, align_horizontal_left, layers, tune, print, mode_fan, thermostat, videocam, folder_open, settings, sync, history, restore, grid_view, send, file_download. NOT flagged (data/brand/photo/illustration per mandate, must stay assets): BambuStudio* logos, helio_icon (Helio brand), printer_preview_*/printer_thumbnail*/monitor_printer (printer photos), bind_machine/unbind_machine, placeholder_pdf/excel/txt, thumbnail_grid/monitor_placeholder/monitor_brokenimg, extrusion_calibration_tips_*/input_access_code_x1_*/ip_address_step/ams_mapping_examples/ams_item_examples/step_mesh_info/cali_*_diagram (instructional diagrams), fd_pattern_*/fd_calibration_*/flow_rate_calibration_*_result (calibration result images), bed_cool/bed_pei/bed_engineering/bed_high_templ/bed_cool_supertack (bed-type product art), Nozzle_HS/HH_* + Big_Nozzle_* (nozzle photos), hum_level*/humidity gauge art, color_picker_border*/transparent_color_picker (functional color UI), flag_red/flag_green/filament swatch art. Read-only audit; no builds/edits. Could not enumerate every one of the 268 ScalableBitmap names into separate rows — grouped by icon family with representative anchors and full member lists inside each gap; an implementer should sweep each family. ImGui/GL texture gaps are architectural (glyph→GL-texture bridge does not yet exist) and are called out as such.
```

---

*登記從 132 個原始審計發現（10 個表面）在 4 個交叉表面合併後整合成 128 個獨特開放缺口。關閉列嘅真實來源：上面嘅表面表；排序：分波計劃；範圍邊界：逐字涵蓋備註。*
