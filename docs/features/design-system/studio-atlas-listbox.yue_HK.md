---
translation-of: studio-atlas-listbox.md
source-sha256: e6b5521b8afdc48f1af1971713d88d31ff1a4d356e51bbfd4a356454ee19296f
review-status: agent-drafted
---

> 英文原文：[Studio Atlas shared list rows](studio-atlas-listbox.md)

# Studio Atlas 共用清單列

呢個有界限單元修改共用自行繪畫 `wxVListBox` 嘅 `Widgets/ListBox.cpp/.hpp`，沿用 Studio Atlas 密度、表面、形狀同動態效果角色。唔會改呼叫端尺寸、列身分、選取、核取狀態、事件合約或持久化。

## 可到達呼叫端同尺寸擁有權

| 呼叫端 | 原有路徑同限制 |
| --- | --- |
| `WorkspacePanel.cpp` | 核對清單頁面、可核取列、160 DIP 最小高度同可擴展佈局。`wxEVT_CHECKLISTBOX` 用索引對應工作區核對清單，並讀取 `IsChecked`。 |
| `Appearance/AppearanceEditorPopover.cpp` | 字型家族同外觀預設搜尋結果，兩個要求高度 132 DIP 嘅清單。原有可見索引映射解析選取；雙擊預設會套用所選預設。 |
| `Schedule/ScheduledSettingsPanel.cpp` | 排程規則清單，要求高度 180 DIP；選取更新狀態，雙擊編輯。 |
| `SmartHomeDialog.cpp` | 喇叭／燈光結果清單，要求高度 150 DIP，沿用原有篩選列。 |
| `SettingsDraftPanel.cpp` | 頁面／草稿選擇清單喺原有 520 乘 440 DIP 對話框內擴展；搜尋將可見列對應至穩定選項。 |
| `Widgets/TabStripDialogs.cpp` | 群組、分頁搜尋同批次關閉結果清單。原有要求尺寸為 360 乘 220、380 乘 180 同 460 乘 240 DIP。原有結果卡片輔助函式將清單高度限制喺顯示工作區三分之一以內。 |

以上係六個呼叫端檔案內嘅九個建立位置。本單元冇修改呼叫端檔案。原生樹狀／資料檢視清單、組合方塊下拉選單同其他清單擁有者唔會繼承呢個實作，仍然分開記錄。

## 列結構同狀態

未自訂清單使用原有舒適模式 14 或緊湊模式 13 內文字型。每列量度目前實際字型同完整多行字串。高度至少符合密度嘅 40 或 32 DIP 下限，文字較高或 20 DIP 核取圖示加垂直內距需要更多空間時會增加。`Rescale` 保留呼叫端明確設定嘅字型；`SetFont` 更新快取列量度。DPI 改變會更新列，唔會指定新控件尺寸。

圓角列面板水平縮入 4 DIP、垂直縮入 2 DIP，使用密度嘅 10 或 8 DIP 小圓角。文字保留原有 12 DIP 內距。核取圖示保留 20 DIP 尺寸同後方 8 DIP 間距，左方起點維持 16 DIP，原有點擊界線維持 36 DIP。每個尺寸只轉換一次像素。寬度分配限制喺列面板內；窄呼叫端冇文字空間時，結果可以係零寬度。空文字矩形唔會送入省略處理。圖示繪畫限制喺實際列矩形內。

選取列配對 `SecondaryContainer` 同 `OnSecondaryContainer`，核取圖示亦一樣。停用嘅選取使用 `SurfaceContainerLow`，文字／圖示使用 `OnSurfaceVariant`。未選取靜止列保留控件背景。懸停使用 `SurfaceContainerHigh`；已選取懸停列加八百分比 `OnSecondaryContainer` 狀態層。已有焦點嘅選取列，喺圓角面板內加 2 DIP `Primary` 焦點環。取得或失去焦點只更新繪畫，唔改選取。

懸停狀態即時更新，繪畫透過共用 100 ms `short2` 時長轉換。動畫綁定擁有清單，銷毀、替換列、清空或重新縮放時停止。減少動態效果透過原有共用路徑即時繪畫終點狀態。動畫回呼只更新有效列，唔會選取、核取、啟動或派發呼叫端指令。

長文字保留原有尾端省略號同完整工具提示。冇新增標籤、翻譯鍵、回呼引擎或資料操作。原有原生方向鍵／Home／End／Page 導航、核對清單 Space 處理、圖示點擊、選取同雙擊事件，以及自訂捲軸擁有權保持不變。本單元冇建立新嘅純鍵盤完整標籤顯示方式，亦冇取代清單原生無障礙實作。

## 針對性驗證

```powershell
node tests/native_shared_controls/atlas_listbox_anatomy.test.mjs --extract "$env:TEMP/BambuStudio-atlas-listbox"
```

九項原始碼檢查涵蓋呼叫端可到達性、字型量度、密度同 DPI 生命週期、核取尺寸保留、選取／焦點狀態配對、有界限繪畫、綁定擁有者嘅減少動態效果回饋，以及對照 `50715f4355e8b845042bd4809bac2da2ce5c40f4` 嘅 17 個基準行為函式內容。替換列同清空函式只多咗重設懸停視覺狀態。負面原始碼檢查會拒絕固定高度文字量度同已改變嘅核取內距常數。

獨立 C++ 測試喺已初始化 x64 MSVC 命令提示字元，編譯直接擷取嘅正式幾何輔助函式：

```bat
cl /nologo /std:c++17 /EHsc /W4 /WX /I"%TEMP%/BambuStudio-atlas-listbox" tests/native_shared_controls/atlas_listbox_geometry_tests.cpp /Fe:"%TEMP%/BambuStudio-atlas-listbox/atlas_listbox_geometry_tests.exe" /Fo:"%TEMP%/BambuStudio-atlas-listbox/atlas_listbox_geometry_tests.obj"
"%TEMP%/BambuStudio-atlas-listbox/atlas_listbox_geometry_tests.exe"
```

四個幾何案例通過，共 24,088 項斷言：正常文字／核取起點；較大或多行字型列高度下限；兩種密度喺 100／125／150／200 百分比縮放及 0 至 500 像素寬度；以及退化尺寸。斷言數包含窮舉組合，唔代表咁多個獨立行為。喺暫存擷取輔助函式移除核取寬度限制，會產生 723 項失敗斷言同退出碼 1。還原正式輔助函式後，全部 24,088 項斷言通過。

冇編譯或啟動原生圖形介面目標。實際選取／核取互動、焦點像素、工具提示傳遞、捲軸行為，以及英文／廣東話／雙語淺色／深色佈局矩陣仍未驗證。冇執行安裝程式、硬件操作或擷取實際畫面。已納入版本控制嘅 Atlas 設計合約喺禁止啟動界線下只係原始碼指引，唔係實際畫面證據。

## 還原同剩餘限制

### 中斷懸停修正

最初懸停實作喺 100 ms 淡出完成前，A 到 B 被 B 到 C 或 B 到離開中斷時，可能令 A 列一直亮住。停止共用動畫器會取消回呼，唔會補最後一次更新。之後取代上一列索引，就失去唯一會更新 A 嘅參照。修正後轉換先記住被取代嘅列，換好繪畫狀態，再喺目前列數界線內令該列重新繪畫，然後先開始新動畫。目前／上一列回呼，以及所有選取／核取處理函式保持不變。

獨立懸停回歸擷取正式 `onMotion`、`onLeave` 同 `animateHover` 函式內容，以及實際懸停繪畫運算式。非視窗轉接器記錄需要重畫嘅列同快取畫面，模擬取消時冇最後更新，並用真正 `MD3MotionPolicy.hpp` 擁有者決策處理正常、減少動態效果同隱藏擁有者模式。佢唔會建立原生控件，亦唔聲稱證明計時器／事件傳遞。六個案例涵蓋兩條中斷路徑同三種動態條件。未修改嘅 `ed3cbc35d5d3a6b701a5eb7ebfdc69d137684d92` 正式函式喺 48 項斷言中有 12 項失敗；修正後全部 48 項通過。原有九項原始碼檢查，包括 17 個保留行為函式內容，仍然全部通過。

```powershell
node tests/native_shared_controls/atlas_listbox_hover.test.mjs --extract "$env:TEMP/BambuStudio-atlas-listbox-hover"
# To reproduce the failing production revision, also pass --source-revision ed3cbc35d5d3a6b701a5eb7ebfdc69d137684d92.
```

喺已初始化嘅 x64 MSVC 命令提示字元執行：

```bat
cl /nologo /std:c++17 /EHsc /W4 /WX /I"%TEMP%/BambuStudio-atlas-listbox-hover" tests/native_shared_controls/atlas_listbox_hover_tests.cpp /Fe:"%TEMP%/BambuStudio-atlas-listbox-hover/atlas_listbox_hover_tests.exe" /Fo:"%TEMP%/BambuStudio-atlas-listbox-hover/atlas_listbox_hover_tests.obj"
"%TEMP%/BambuStudio-atlas-listbox-hover/atlas_listbox_hover_tests.exe"
```

呢項獨立修正只改中斷懸停嘅重畫處理。還原會重新引入殘留列畫面。原生編譯、計時器傳遞同實際懸停畫面仍未驗證。

呢個係獨立共用列外觀同量度佈局單元。還原會恢復之前固定列高度、13 內文字型預設、窄文字矩形缺乏界限嘅行為，同原有列狀態繪畫。唔需要一併還原呼叫端卡片、選取模型或中性色配色。

呼叫端尺寸改動、新增持續顯示嘅輔助標籤、純鍵盤完整文字顯示，以及其他清單實作仍然唔屬於呢個範圍。九個可到達建立位置證明原始碼覆蓋，唔代表執行期間一致，亦唔代表全應用程式重設計完成。
