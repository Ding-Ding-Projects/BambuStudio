---
translation-of: studio-atlas-shared-controls.md
source-sha256: b80f4d1851599099f61df69822d30da389b3b17ee2943ca6e70463cc16e51f7f
review-status: agent-drafted
---

> 英文原文：[Studio Atlas shared control anatomy](studio-atlas-shared-controls.md)

# Studio Atlas 共用控件結構

純外觀單元以 `be5e1205dcdad8f372d2ba63f367dbd7977c29c4` 為基準，改善四個原生基礎控件，冇取代命令、輸入或資料模型。完整應用程式重設計仍超出呢啲共用預設。

## 實作處理

| 控件 | 外觀 | 保留邊界 |
| --- | --- | --- |
| Button | 操作變體由膠囊改密度圓角矩形，明確圓形保留；填色／次要底色用 8% 懸停、12% 按下前景層並限制對比；外框／文字／圖示有不同語意狀態，勾選及停用優先清楚 | 尺寸級、量度標籤／圖示、內距、輸入、回呼、無障礙身份、呼叫者變體 |
| StaticBox | 建立時預設半徑跟密度，互動沿用 100 毫秒，繪畫半徑限制喺真實矩形 | TextInput／SpinInput 等明確半徑優先；邊線／填色、漸變、徽章、懸停擁有權、減少動態 |
| SearchField | 低層圓角欄位，形狀外用父背景，輸入及子操作共用內部角色；兩 DIP 焦點外框唔移動控制 | 44 DIP 欄位、40 DIP 操作、輸入／清除／切換／構建器位置、查詢界限、引擎、回呼、焦點 |
| MD3Menu | 懸停／鍵盤選取畫內縮圓角面，選取有短前導標記，彈出圓角跟密度 | 原完整點擊矩形、圖示／標籤／捷徑／箭嘴欄、捲動／篩選、身份、啟用／勾選、派發、焦點返回、減少動態 |

搜尋兩 DIP 線中心離外緣一 DIP，原 40 DIP 子目標喺 44 DIP 欄內兩 DIP 起步，所以線唔遮住子目標。呢個係一般內縮焦點參考嘅明確幾何調整。Button 保留原兩 DIP 焦點線及對比備援。

強調狀態層若會令文字低於 4.5:1，就降低裝飾透明度；原本已低於 4.5:1 嘅用戶色對，唔會令佢更差。唔改用戶色或宣稱修好低對比；冇安全混色時保留原面。語意狀態同回呼即時生效，冇新計時器、畫布淡出、延遲啟動或持續裝飾。

## 覆蓋同呼叫者缺口

頁面包含 Button 或 SearchField 唔等於完整重設計。

| 邊界 | 例子 | 剩餘工作 |
| --- | --- | --- |
| 明確樣式按鈕／導覽 | `Notebook.cpp`、`Widgets/TabCtrl.cpp`、`Widgets/TabStrip.cpp` | 呼叫者擁有圓角、背景、狀態，逐一審查，預設唔可以覆蓋 |
| 文字／選擇／數字 | `Widgets/TextInput.cpp`、`Widgets/ComboBox.cpp`、`Widgets/SpinInput.cpp` | 標籤／值／說明／驗證堆疊屬其他工作，SearchField 仍用提示衍生無障礙名稱 |
| 其他渲染器 | `Widgets/SideMenuPopup.cpp`、`Widgets/CheckBox.cpp`、`Widgets/SwitchButton.cpp`、`GLToolbar.cpp`、`ImGuiWrapper.cpp` | 獨立繪畫／輸入同真實互動 |
| 卡片組合 | `ParamsPanel.cpp`、`StatusPanel.cpp`、`Preferences.cpp` 及子面板 | 標題／內容／頁尾、文案、分隔、內距、捲動唔會由圓角自動提供 |
| 自訂尺寸級 | Small／Medium／Large | 保留 36/42/44 DIP 同字號，40/32 DIP 密度遷移係後續工作 |
| 長選單／雙語 | `MD3MenuList` | 保留量度、最大寬同次標籤展示，唔宣稱省略或最長標籤符合最終無裁剪合約 |
| 內嵌／畫布 | `resources/web`、`DeviceWeb`、OpenGL／ImGui | 共用原生控件唔證明其他畫面或互動 |

## 有限驗證

`node --test tests/native_shared_controls/atlas_control_anatomy.test.mjs` 七項來源案例涵蓋狀態優先、尺寸不變、焦點線幾何、選單邊界、呼叫者半徑、減少動態及 15 個不變輸入／回呼／量度方法。記錄雜湊喺建立 manifest 前同基準核對。刻意移除按下狀態同改欄位寬度必須被拒絕，再由原來源通過。唔代表原方法毫無缺陷。

可獨立編譯實際產品顏色計算：

```powershell
node tests/native_shared_controls/atlas_control_anatomy.test.mjs --extract $scratch
# Use the supported MSVC x64 environment and a temporary directory for $scratch.
cl /nologo /std:c++17 /EHsc /W4 /I tests /I $scratch tests/native_shared_controls/atlas_button_state_tests.cpp /Fe:$scratch/atlas_button_state_tests.exe /Fo:$scratch/atlas_button_state_tests.obj
& $scratch/atlas_button_state_tests.exe
```

抽取逐字複製亮度、對比及狀態層函數並輸出 SHA-256；C++ 用最小 RGB 轉接器而唔係 wxWidgets，測預設強調色及固定自訂色網格，只證明計算邊界，冇啟動視窗。

原生編譯、擷圖、焦點繪畫、主題切換、讀屏、長翻譯、自訂外觀同動態時間未驗證。獲准啟動後仍要正常／最小、明暗、兩密度、三語言、100/125/150/200%、系統／減少動態矩陣。

## 還原

同較早導覽及配色分開。審查後還原本提交只移除共用外觀、有限測試及本文，保留 Print 路由、穩定頁 ID、打印機／AMS、建置同獨立配色修正。中央帳本記錄整合提交，唔包括重設或改寫歷史。
