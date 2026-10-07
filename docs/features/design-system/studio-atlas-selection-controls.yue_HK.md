---
translation-of: studio-atlas-selection-controls.md
source-sha256: 82b93f8a2ba4e5ab230f4cec26f655a2d2b1962c772ce34e3f6d7763824bfddd
review-status: agent-drafted
---

# Studio Atlas 數值及選擇控制項

[English](studio-atlas-selection-controls.md)

今次只處理 `SpinInput`、`CheckBox` 同 `SwitchButton`，沿用
[`design/workflow-refresh.md`](../../../design/workflow-refresh.md) 嘅角色及原生設計套件。
對照版本係 `8ce0ad44698d59ccf942518a56645b37d3f9b9eb`。
目前唔准啟動 Material Designer 或原生應用程式，所以只完成原始碼及針對性檢查。
原生編譯、實際畫面、輔助功能操作及設計比對仍未驗證。
56 幅結構設計圖唔係畫面截圖；1,204 項功能責任嘅狀態維持不變。

## 控制項結構

- 數值欄用低層容器底色、靜止外框同焦點主色。左邊加減控制同編輯區分開，
  用實際字型量度數值及單位。舒適模式嘅加減欄寬及邊距係 20/4 DIP，緊湊模式係
  18/3 DIP。最小寬度保留一個數字加兩 DIP，較長數字仍由原生編輯器捲動。
  太窄嘅呼叫端要求會按量度結果增加寬度，容器是否容得下仍要實際驗證。
- 核取方塊保留 20 DIP 佔位，圓角改為四 DIP，勾號置中並縮至 16 DIP。
  半選橫線、備用筆畫、停用及焦點圖像、呼叫端底色同減少動態效果路徑保留。
- 有文字嘅切換掣用等寬分段同兩 DIP 內距，字型跟隨呼叫端。
  每段水平邊距係八或五 DIP，高度按字型加六 DIP 同密度列高減八 DIP 取較大值。
  已設定嘅最大寬度及縮字行為保留；未設定嘅最大寬度唔會當成負數圖像寬度。
  極窄限制下嘅可讀性未有實際畫面證據。
- 無文字切換掣嘅關閉底色改用低層容器，保留 44 乘 24 DIP 軌道、圓形滑塊、
  150 毫秒過渡、停用狀態同減少動態效果時直接切換嘅路徑。

數值解析、範圍、鍵盤、按住重複、事件、選取狀態、雙語提示同呼叫端顏色覆寫都保留。
冇加產品字串、翻譯鍵、計時器、功能引擎或回呼。標頭 API 冇改。
`SwitchButton.cpp` 內其他類別全部維持原樣。

## 原生尺寸分配修正

首個版本只喺量度外框時擺放子控制項。原生 sizer 用五參數 `SetSize` 分配尺寸，
唔會經過 `SpinInput::SetSize(wxSize)`，所以縮窄或降低後，子控制項仍留喺舊位置。
修正喺兩個加減子控制項建立後才連接 `wxEVT_SIZE`。處理函式只按 `GetClientSize()`
重新擺位，再傳遞事件；唔會改外框大小或最小尺寸，亦有重入保護。
量度同最小尺寸公告分開，明確要求嘅初始尺寸或便利多載尺寸以 DIP 保留為最小值，
未指定嘅尺寸用內容量度值。即使被強制分配低於最小值，都唔會沿用越界舊座標或負數大小。

## 檢查及限制

英文文章列出完整可重現命令。八項原始碼檢查保護 40 個原有方法及其他類別嘅完整內容，
建構方法只容許新增尺寸記錄及事件連接兩行。故意破壞範圍儲存、其他類別、字型量度、
最小寬度報告、尺寸事件連接或加入外框尺寸遞迴，都會令檢查失敗。
抽出實際數值排版及尺寸事件方法嘅 C++ 檢查通過 7,296 項斷言，涵蓋兩種密度、
100%、125%、150%、200% 比例、零至 800 DIP 要求寬度、四種高度同長短單位。
呢個結果唔等於 wxWidgets 編譯成功，更唔等於真實文字或畫面已驗證。
最小視窗合約亦驗證 96/110 DIP 初始配置縮到公布最小值、強制縮窄及降低、零面積同再次放大。
舊子控制項原本超出縮細後嘅範圍，事件處理後三個子控制項都留喺實際配置內。
對 `b37bb7e917398685cb56d2f2f13a5abd0b47048d` 跑新檢查，結果係六項通過、兩項失敗；
修正後八項全部通過。呢個係原始碼生命週期證據，唔係實際 wxWidgets 操作證據。

現有 `ui-md3/tests/context-menus.test.mjs` 有五項通過、一項範圍外失敗：
`CameraHUD.cpp` 仍呼叫 `m_zoom_percent->PopupMenu`。該檔案同基準版本完全相同，
今次冇改。文字編輯及數值欄嘅選單檢查通過，但唔會把整套結果報成成功。

設計收據檢查命令係 `node design/workflow-refresh/check-surface-contracts.mjs`。
當中固定版本嘅收據唔包括今次新增控制項嘅實際畫面驗收。

下一步要檢查物件顏色、Prepare、匯出及批次對話框嘅窄數值欄，
以及 Params、Preferences、相機面板嘅切換掣最大寬度。標籤、可點擊整列、
說明文字、`LabeledCheckBox`、`ImageSwitchButton`、`SwitchBoard` 同其他自訂切換控制
仍由各自範圍處理。焦點朗讀、鍵盤次序、即時密度及主題變更要等實際應用程式證據。
獲准啟動後仍要完成正常及最小尺寸、三種語言、兩種主題同四種比例嘅完整矩陣。

## 可撤回範圍

今次同時有外觀及最小尺寸排版改動，唔係單純換色。
如果維護者唔鍾意新設計，可以審閱後撤回呢個獨立版本連同檢查及文章。
保留其他建置、AMS、Print、導覽及資料修正；唔會自動回復，亦唔會重設到舊設計基準。

## 重現命令及保留細節

喺儲存庫根目錄執行 `node --test tests/native_controls/atlas_selection_anatomy.test.mjs`；用 `node tests/native_controls/atlas_selection_anatomy.test.mjs --extract "$env:TEMP/atlas-selection-geometry"` 抽出正式方法。舊版比較命令係 `node tests/native_controls/atlas_selection_anatomy.test.mjs --source-revision b37bb7e917398685cb56d2f2f13a5abd0b47048d`。

最小高度依實際編輯器或單位字型加垂直邊距決定；明確指定嘅圓角喺縮放後仍有效。核取方塊保留布林／半選狀態、情境強調色及主題刷新；文字切換掣保留兩個標籤次序、原生切換事件、完整雙語提示同內外圓角關係。滑鼠滾輪、擷取釋放及文字／數值事件亦保留。

喺支援嘅 MSVC 開發者命令提示字元，只編譯抽出嘅正式幾何方法同針對性測試，輸出到暫存目錄：

```bat
cl /nologo /EHsc /std:c++17 /I"%TEMP%\atlas-selection-geometry" tests\native_controls\atlas_selection_geometry_tests.cpp /Fo"%TEMP%\atlas-selection-geometry\geometry.obj" /Fe"%TEMP%\atlas-selection-geometry\geometry.exe"
"%TEMP%\atlas-selection-geometry\geometry.exe"
```
