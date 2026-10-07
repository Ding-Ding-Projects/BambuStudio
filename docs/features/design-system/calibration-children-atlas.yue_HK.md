---
translation-of: calibration-children-atlas.md
source-sha256: f8a3163d0be426aa6b2707a94263df73d8e1dda2bea781512041c2141effdba1
review-status: agent-drafted
---

> 英文原文：[Studio Atlas calibration child surfaces](calibration-children-atlas.md)

# Studio Atlas 校準子頁面

呢個有限範圍嘅外觀更新跟隨設計版本 `3c8fe2708` 嘅 `calibration` 合約，涵蓋 `CalibrationWizardPage.cpp`、`CalibrationWizardStartPage.cpp`、`CalibrationWizardPresetPage.cpp`、`CalibrationWizardCaliPage.cpp` 同 `CalibrationWizardSavePage.cpp`。

## 改動同保留行為

| 頁面 | 外觀改動 | 保留行為 |
| --- | --- | --- |
| 共用標題同流程指示 | 跟密度調整間距、真實步驟標籤換行、次要狀態同選取底色分明 | 標籤、數目、次序、選取索引同重建流程 |
| 共用操作組 | 原有操作可以換行，移除會爭位嘅比例伸展空位 | 建立次序、啟用條件同實際發送嘅操作 ID |
| 開始頁 | 加強章節標題，說明文字退一級 | 指示、插圖、校準方法同裝置能力判斷 |
| 預設頁 | 分開讀數同建議區，加強校準類型及打印參數標題 | 材料、噴嘴、範圍、單位、驗證、同步同打印機身份 |
| 校準進行中 | 真實打印面板同後續操作之間留白 | 遙測、進度、暫停／繼續、中止確認同硬件回呼 |
| 儲存／結果頁 | 結果標題、讀數表面同部分失敗區更清楚 | 結果對應、因子值、命名、儲存決定同預設更新 |

中性色沿用語意角色，冇新增翻譯鍵、文案、引擎、打印命令、回呼、計時器或者動畫。焦點同減少動態效果行為保留。移除嘅步驟連線裝飾由真實有序標籤同選取底色取代，冇捏造較短流程。

## 來源驗證

執行 `node --test tests/calibration_children_atlas.test.mjs`。五項來源檢查以 `34fa40252bbb9b755fd22eb503280a045d8923ee` 為基準；只排除明確繪畫同排版語句後，非視覺程式令牌一致，所有字面值、翻譯鍵、單位同流程文字一致。檢查保留步驟身份同頁尾組合，會拒絕刻意修改嘅操作 ID 同結果方法；掃描器支援索引目標同跳脫多行字串。呢啲唔係 C++ 編譯或執行證據。

## 尚欠證據同限制

冇啟動應用程式、瀏覽器、安裝程式或打印機，冇完整建置、擷圖、原生控件量度或者執行驗收。明確禁止啟動嘅限制亦令即時設計流程不可用。

原有 1100 DIP 校準頁最小寬度、固定指示文字寬度、多欄預設／結果表格同捲動擁有權不變。可換行排版唔代表每種翻譯喺最小視窗都放得落。操作組仍然跟頁面捲動，冇另外固定頁尾。最小尺寸、語言、主題、比例、鍵盤、焦點同真實流程切換仍待驗證；五個檔案嘅更新唔等於整個校準合約完成。呢個單元可以獨立還原，唔應移除設定索引、校準算法或打印機行為；整合負責人提供建置同原生證據。

## 完成標籤接收者修正

最初 `d3821d9bcbbc6e90fb9b941013ab7b8039534c68` 喺 `CaliPASaveManualPanel::create_panel` 同 `CaliPASaveP1PPanel::create_panel` 用咗未宣告嘅本地接收者設定前景色。後續只將兩處改為已宣告及初始化嘅 `m_complete_text`，四個有效本地接收者保留。

`node --test tests/calibration_label_scope.test.mjs` 讀取真實方法範圍同所屬類別宣告。原版六項通過、兩項失敗；修正後八項全通過。負向案例拒絕範圍外、較後宣告、未宣告同未初始化接收者；檢查保留前景色呼叫。呢個係有限來源回歸，唔係完整 C++ 名稱解析器或原生編譯證明，修正期間冇建置或啟動應用程式。
