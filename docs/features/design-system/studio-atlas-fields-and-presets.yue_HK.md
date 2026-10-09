---
translation-of: studio-atlas-fields-and-presets.md
source-sha256: 844b517cfc391d1d4c1c4cf2845ebde4936ce01e6717f74205ce7d32ddb22660
review-status: agent-drafted
---

> 英文原文：[Studio Atlas fields and preset controls](studio-atlas-fields-and-presets.md)

# Studio Atlas 欄位同預設控制

呢個外觀單元按 Atlas 合約，接續共用控件繪畫同瓷白／石板配色，實作原有欄位／預設組合，唔代表整個應用程式重設計完成。

## 改動範圍

| 來源 | 實際覆蓋 |
| --- | --- |
| `Widgets/TextInput.cpp` | 填色欄位、密度圓角／最小列高、內縮焦點線、量度編輯器／前綴／單位／標籤／說明、驗證前後景配對 |
| `Widgets/ComboBox.cpp` | 唯讀選取面、懸停／按下／焦點、情境焦點文字配對、隱藏編輯器及設定字型後重新量度 |
| `BitmapComboBox.cpp` | Windows 自畫項目背景同原生矩形內圓角選取面 |
| `Field.cpp` | 單行選項最小值、多變體前綴內距／行距、DPI 後恢復變體寬度時保留量度下限 |
| `Tab.cpp` | 預設標題密度間距／最小高，建立、縮放、主題更新時由邏輯尺寸刷新 |

每個字串按實際繪畫字型量度。說明喺原標籤下面同一水平位置；前綴及單位用編輯器字型。單一計算只預留每部分一次，提供必需最小寬度。分配不足時編輯器寬度唔會變成原生 `-1` 預設尺寸哨兵。低於量度下限嘅強制分配由繪畫裁剪限制，但仍要修正所屬父版面；最小寬度計算唔證明所有父項都遵守。

舒適／緊湊沿用 40/32 DIP 列高下限同 10/8 DIP 圓角；文字堆疊或圖示需要時加高。內距跟活動密度，欄內間距八 DIP。計算輸入係裝置像素，唔二次換算。字型、前綴、標籤、圖示、密度及同尺寸更新都會重新量度並重新定位編輯器；明確指定圓角仍作準。

標題仍用原面板／sizer，預設、搜尋、儲存、刪除、匯出、還原身份及事件不變。Bitmap 選擇仍用原生文字／圖像繪畫，冇取代列高、項目資料或鍵盤。

## 狀態同保留行為

唯讀選擇靜止／懸停／按下分別用低／高／最高容器；焦點用情境次要容器及配對文字，停用優先。原共用狀態過渡負責回饋同減少動態，冇新增計時器或為動畫延遲事件。

驗證保留檢查器、提示及可選對話框。無效文字用錯誤容器配對文字，成功後恢復呼叫者當前狀態色，唔用硬編中性色。冇新增驗證引擎、訊息、持久化鍵、預設繼承、搜尋 ID 或設定註冊。

## 呼叫者剩餘工作

- 持續選項標籤、重設／繼承標記、獨立驗證列、窄版堆疊、分類卡標題／頁尾仍屬選項列及面板；本單元只用已有 `TextInput` 說明。
- `OptionsGroup`、`ParamsPanel`、`PresetComboBoxes`、`GUI_ObjectList` 各自擁有組合、尺寸及覆蓋，需要獨立驗證。
- `SpinInput`、多行 `TextAreaEditor`、滑桿、資料色塊、專用編輯器有獨立繪畫／版面；外層欄位唔證明全部覆蓋。
- `BitmapComboBox` 只改 Windows，自有 macOS 繪畫及 GTK 行為未重設計或驗證。
- 特長唯讀標籤／說明會要求較大最小寬度，父捲動、換行及最小視窗要喺建置程式驗證。
- 驗證仍係原提示／對話框，冇實作持續行內驗證列。

## 有限驗證

```powershell
node tests/native_shared_controls/atlas_field_anatomy.test.mjs --extract "$env:TEMP/BambuStudio-atlas-field-layout"
```

喺已初始化 x64 MSVC 命令列，用抽取目錄作 include，獨立編譯幾何測試：

```bat
cl /nologo /std:c++17 /EHsc /W4 /WX /I"%TEMP%/BambuStudio-atlas-field-layout" tests/native_shared_controls/atlas_field_layout_tests.cpp /Fe:"%TEMP%/BambuStudio-atlas-field-layout/atlas_field_layout_tests.exe" /Fo:"%TEMP%/BambuStudio-atlas-field-layout/atlas_field_layout_tests.obj"
"%TEMP%/BambuStudio-atlas-field-layout/atlas_field_layout_tests.exe"
```

七項來源檢查通過，包括拒絕缺少重新排版嘅刻意變異，以及同 `df300fb9d93991751e840bdc586d2f770b47b9cf` 比較嘅 22 個逐位元組一致行為方法。編譯後產品幾何方法五個案例、1,428 個斷言通過，涵蓋純編輯器、左右標籤堆疊、唯讀、不足分配、長文字密度／比例組合。斷言數唔等於獨立行為數。

只係來源同計算證據，冇建置或啟動應用程式。原生 wxWidgets 編譯、畫面、鍵盤同三語言／明暗／100%、125%、150%、200% 矩陣仍未驗證。禁止啟動令 Material Designer 即時流程不可用，已提交合約只係實作參考。

## 還原

可獨立還原外觀／排版提交，冇設定遷移或外部 API。恢復舊欄位幾何、預設間距同選擇繪畫時，保留較早導覽、工作流及打印行為。唔需要還原其他導覽提交；若之後呼叫者依賴新最小值，還原前要另外審查。

## 預設標頭間隔編譯修正

原生建置報告 C2039，原因係 wxSizerItem 冇 SetSpacer 成員。applyPresetHeaderAnatomy 而家改用已安裝 wx 提供嘅 SetMinSize(int, int)。實際標頭只寫入 m_minSize，唔會取代項目或佢擁有嘅間隔；已檢視嘅 wx 實作喺 CalcMin 保留呢個最小尺寸，喺 SetDimension 將排版尺寸套用到原有間隔。父 sizer 擁有權、由 DIP 計算嘅內距、高度 1，同密度／DPI 更新呼叫都不變。

喺已初始化嘅 MSVC 開發環境執行 node tests/native_shared_controls/preset_spacer_compile.test.mjs --wx-root <installed-wx-prefix>。檢查會抽取實際產品間隔陳述式，對住真正安裝嘅 wx/sizer.h 同配套設定標頭編譯。加上 --source-revision c1149eab941cb179f3333f994111899281627ffb --expect-missing-api 可重現 SetSpacer 嘅 C2039；修正後嘅陳述式編譯成功。冇用替代 wx 類別或假兼容方法。現有針對欄位／預設密度及 DPI 生命週期嘅源碼檢查亦通過。

喺非 Windows 平台可以唔畀 `--wx-root`：測試會用本機 C++ 編譯器（`CXX`，預設 `c++`）對住 `wx-config` 指出嘅已安裝 wx 標頭編譯同一句陳述式，預期失敗模式接受該編譯器對 `SetSpacer` 嘅缺少成員診斷。對 Windows wx 建置嘅 MSVC 執行仍然係 Windows 證明。

呢個結果只驗證修正後嘅 API 呼叫，唔代表完整 Tab.cpp 編譯單元、已連結應用程式、執行時間隔排版或實際介面已驗證。呢個修正工作冇做完整建置或啟動應用程式。
