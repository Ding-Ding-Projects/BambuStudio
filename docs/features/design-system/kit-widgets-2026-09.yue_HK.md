---
translation-of: kit-widgets-2026-09.md
source-sha256: 4fb4f6882d95bc91d01c87531da5609f89a8fea0c733f2dd908f25542965063c
review-status: agent-drafted
---

> 英文原文：[Kit widgets added in the every-element sweep (2026-09-05)](kit-widgets-2026-09.md)

# 喺每元素掃過中添加嘅工具組件（2026-09-05）

四個工具組件同一個工具組件延伸到達所以冇庫存 wxWidgets 控制必須保留喺一個用戶面向表面。每個鏡像一個現有工具原語所以呼叫者需要冇新詞彙，同每個係釘住由一個原始碼讀取合同喺 `ui-md3/tests/md3-conversion-contracts.test.mjs`。

## LabeledRadioButton 同 RadioGroup（`src/slic3r/GUI/Widgets/LabeledRadioButton.{hpp,cpp}`）

**行為。** 一個可聚焦列嘅繪製 `RadioBox` 字形加一個 `Label`。點擊字形、標籤或列、或按 Space / Enter 當列有焦點時，選擇佢同排放 `wxEVT_RADIOBUTTON` 從列（每個用戶啟動，似乎原生 MSW 控制）。一列喺佢自己可以選擇但唔會由用戶取消選擇。`RadioGroup` 係一個純事件處理器對話框擁有：`Add()` 成員，同佢強制相互排除、隨著上 / 左、下 / 右、主頁同結束移動選擇同焦點、跳過隱藏或禁用成員，同報告 `GetSelection() == -1` 當冇嘢被選擇。`SetSelection(-1)` 清除列以程式方式。

**無障礙。** 列帶著一個 `wxAccessible` 對等物：角色 `ROLE_SYSTEM_RADIOBUTTON`、名稱從標籤、狀態可聚焦 / 聚焦 / 已檢查 / 不可用 / 不可見、預設行動「選擇」。一個 2 px 初級焦點環係繪製圍繞字形。字形本身係唔可聚焦。

**配置。** `SetColorScheme()` 將選擇點重新著色到預覽或設備重點；`Rescale()` 喺 DPI 改變；`SetLabel("")` 隱藏標籤用於純字形列。

**失敗模式。** 一列添加到兩個組被第二個 `Add()` 忽視。摧毀成員從其組移除佢。程式方式 `SetValue()` 永遠唔排放。如果組係擁有嗰啲列嘅視窗嘅成員，佢會喺 wx 摧毀嗰啲列之前先被摧毀；佢嘅解構函數會解除自己加過嘅所有處理器，連摧毀處理器都包埋，所以冇任何列事件會去到已經摧毀咗嘅組。

**網站。** FeedDirectionDialog、CalibrationWizardPresetPage（階段對同每個位置選擇器）、SavePresetDialog、元素鎖精靈（解鎖方式同時長）、本機支援工單（類別）、排程設定（數值來源）。

## LabeledCheckBox 無障礙（`src/slic3r/GUI/Widgets/LabeledCheckBox.{hpp,cpp}`）

**行為。** 取代原生 `wxCheckBox` 嘅一列：工具組件 `CheckBox` 字形，後面跟住一個 `Label`。字形攞鍵盤焦點，撳 Space 會切換佢；撳字形或者標籤都會切換；每次用戶切換都會由嗰列發出 `wxEVT_CHECKBOX`。

**無障礙。** 字形係一個冇自己文字嘅原生按鈕，而 wxWidgets 唔會自動幫視窗建立無障礙物件，所以淨係 `SetName()` 從來去唔到讀屏軟件。因此字形帶住一個 `wxAccessible` 對等物：角色 `ROLE_SYSTEM_CHECKBUTTON`，狀態有可聚焦 / 焦點 / 已剔 / 不可用 / 不可見，預設動作係「剔選」或者「取消剔選」，效果同撳一下一樣，會切換同發出 `wxEVT_CHECKBOX`。名稱喺每次被問嘅時候先計：調用者用 `SetName()` 幫字形或者成列設定嘅名；冇就用嗰列顯示緊嘅標籤（套用個人詞彙，去走助記符）；再冇就用提示文字。`SetLabel()` 會發出名稱改變事件，每次切換都會發出狀態改變事件。

**網站。** 所有 `LabeledCheckBox`，包括排程設定（啟用、每日、七日星期、三個來源同意列同布林數值編輯器）、身份記錄嘅動作篩選，同元素鎖精靈嘅聲明。

## TextArea（`src/slic3r/GUI/Widgets/TextArea.{hpp,cpp}`）

**行為。** 工具組件多行欄位：一個邊框容器裝著一個無邊界原生編輯器通過 `GetTextCtrl()` 到達。OutlineVariant 1 px 喺休息、初級 2 px 當編輯器有焦點、`radius_tiny` 角、SurfaceContainerLowest 填充當可編輯同 SurfaceContainerLow 當唯讀。`SetMonospace(true)` 交換編輯器到 Roboto Mono 用於 JSON、日誌同腳本。`SetMinLines()` 驅動最佳尺寸當父級唔尺寸欄位。

**失敗模式。** 呼叫者需要 `SetStyle()` 或 `wxTE_RICH` 保持用佢哋喺內部控制上。容器唔滾動；編輯器做。

**網站。** 更新對話框更改日誌、訊息對話框腳本主體、系統資訊 JSON、網絡測試日誌、打印評分評論、兩個 WebViewDialog 開發者查看器、未儲存改變差異單元格。

## ListBox（`src/slic3r/GUI/Widgets/ListBox.{hpp,cpp}`）

**行為。** 一個所有者繪製 `wxVListBox` 繪製帶著下拉選單解剖：SurfaceContainer 欄位、圓形 SurfaceContainerHigh 懸停窗格、SecondaryContainer 選擇活動計劃中嘅窗格、OnSurface 文字喺工具組件主體面孔。原生鍵盤模型同 `wxEVT_LISTBOX` 被繼承。長列省略喺末尾同懸停列暴露其完整文字作為提示。

**延伸選取。** 用 `wxLB_MULTIPLE` 建立嘅清單取代用 `wxLB_EXTENDED` 建立嘅 `wxListBox`：`wxVListBox` 提供延伸選取模式（撳一下揀一列，Ctrl+撳一下切換一列，Shift+撳一下同 Shift+方向鍵延伸範圍，Ctrl+方向鍵移動目前嗰列但唔會揀佢，Ctrl+Space 切換目前嗰列，單獨撳 Space 就淨係揀目前嗰列），而 `GetSelections()` 喺兩種模式都按次序交返已揀嘅列，同 `wxListBox::GetSelections()` 一樣。`SetSelection(n)` 會將第 `n` 列加入多重選取，`Clear()` 會清空佢。

**焦點環。** 清單有鍵盤焦點嘅時候，目前嗰列（即係鍵盤所在嗰列）入面會畫一個 2 px 嘅 Primary 環，唔理嗰列有冇揀到；其他已揀嘅列就淨係保留 SecondaryContainer 底色。單選清單入面，目前嗰列就係已揀嗰列。要揀兩列唔相連嘅列，用 Ctrl+方向鍵移過去，再喺每一列撳 Ctrl+Space。

**無障礙。** 清單帶住一個 `wxAccessible` 對等物，好似原生清單咁樣向讀屏軟件交代自己：角色 `ROLE_SYSTEM_LIST`，名稱用 `SetName()` 設定嘅名；每一列係一個 `ROLE_SYSTEM_LISTITEM` 子項，名稱係嗰列嘅文字，狀態有可揀、已揀、焦點（清單有焦點時嘅目前嗰列）、已剔（剔選列）、畫面外（捲咗出視窗）同不可用。多重選取清單仲會標明可多選同可延伸選取。對等物會報告每列位置、點擊測試、鍵盤焦點同已揀嘅列；當按鍵或者撳滑鼠移動咗目前嗰列或者改咗選取，以及 `SetSelection()` 之後，清單都會發出焦點同選取事件。

**失敗模式。** 多重選取清單啱啱清空再填過之後，未有目前嗰列，要等按鍵或者撳滑鼠先會有，所以佢第一次攞到焦點嘅時候唔會見到焦點環。

**網站。** 智能家庭實體清單；排程設定嘅規則（延伸選取）、可用設定、設定選項同時區；驗證器項目、本機支援工單同身份記錄版本（延伸選取）。

## Button::SetIconBitmap 同 ScalableBitmap(wxWindow*, wxBitmap)

`ScalableBitmap` 可以包裹一個位圖呼叫者已經渲染（一個顏色樣本）；佢冇圖示名稱同 `msw_rescale()` 讓佢單獨。`Button::SetIconBitmap()` 展示這樣一個位圖喺每個狀態，所以顏色樣本同其他資料影像可以住喺工具組件圖示按鈕內冇被誤認為圖示資源。

## Button 預設係 Material

一個 `Button` 如果到第一次畫嘅時候都冇用 `SetVariant()` / `SetIconButton()` 揀款式，亦冇被調用者打扮過，就會用 Outlined 款式。每個明確設定樣式嘅方法（背景、邊框、文字顏色、圓角半徑）都會將 Button 標記做調用者打扮過，所以手工打扮嘅按鈕保持原本樣子。Outlined 同 Text 款式有已選取狀態：SecondaryContainer 填色、OnSecondaryContainer 標籤。

Outlined 款式有自己嘅標籤字體同每邊 18 DIP 嘅留白，所以通常喺父視窗嘅排版器擺好位之後先令按鈕變闊。因此第一次畫嘅時候會比較款式前後嘅最細尺寸，有變就排隊將父視窗重新排版一次：同一輪幾多粒按鈕換款式，每個父視窗都只排一次，而且淨係處理由排版器擺位嘅按鈕。`e5faf503d` 之前冇人再問過，智能家居嘅「Close」掣喺英文同粵語模式一直被擠到細過佢嘅最細尺寸（版面裁剪清單 CJ-029）。

## 驗證

- `node --test ui-md3/tests/md3-conversion-contracts.test.mjs` 固定每個組件嘅登記、結構、無障礙角色，同埋對應原生控件允許清單係空嘅。
- `node --test ui-md3/tests/button-first-paint-layout.test.mjs` 固定第一次畫換款式之後嘅父視窗重新排版。
- 執行時擷圖（淺色同深色、英文 / 粵語 / 雙語、100 至 200 百分比）喺構建好嘅應用程式存在之後記錄喺 `docs/screenshots/md3-everything/`；喺嗰之前，`md3-parity-register.md` 入面嘅行會講明。

## 建議文章

- [MD3 設計系統](md3-design-system.md)
- [MD3 奇偶校驗登錄](md3-parity-register.md)
- [主題表面顏色](themed-surface-colors.md)
