---
translation-of: embedded-composition-atlas.md
source-sha256: 48c15500aebd2e8840d0580d65f59f9673f6e927152ee46bb72c9129f937c7d5
review-status: agent-drafted
---

> 英文原文：[Studio Atlas embedded composition](embedded-composition-atlas.md)

# Studio Atlas 內嵌頁面組合

呢個只涉及 CSS 來源嘅單元跟隨 `3c8fe270` 嘅 `design/workflow-refresh/surface-contracts.json`，基準係 `8cfce63ae05d7be03823b3eb9e4b86c4488245b1`。JavaScript、HTML、橋接訊息、認證、網絡、第三方樣式同獨立網站都不變。

## 入口覆蓋

以下九個 `resources/web` HTML 入口合共有十四個樣式匯入：

| 入口 | 樣式 | 外觀組合 |
| --- | --- | --- |
| `homepage3/home.html` | `homepage3/css/home.css`、`recent.css`、`online.css`、`manual.css` | 完整最近檔案清單改響應式網格，分開標題、名稱、資料、狀態列同指引說明 |
| `homepage3/left.html` | `homepage3/css/left.css` | 帳戶／導覽分組，目的地文字換行，原迷你側欄斷點以上顯示選取標記 |
| `homepage3/wiki.html` | `homepage3/css/wiki.css` | 搜尋／分頁分組，主題卡取消固定 600px 最小值，搜尋結果可捲動，外距收窄 |
| `model_new/index.html` | `model_new/css/gallery.css`、`navigation.css` | 畫廊框、縮圖選取邊界、窄版直向縮圖列、導覽底色同焦點 |
| `model_new/editor.html` | `model_new/css/tool.css`、`accessory_dropdown.css` | 原通知訊息／操作對齊、配件清單內部捲動、標籤／值對齊 |
| `filament_create/step2.html` | `filament_create/step2.css` | 參數工作區、分頁／分類換行、可讀參數列、可捲內容同可到達頁尾 |
| `guide/1/index.html` | `guide/1/1.css` | 按內容量度歡迎標題同說明，主要操作獨立成區 |
| `guide/23/index.html` | `guide/23/23.css` | 篩選群、分類換行、選取分頁、自訂耗材列，保留編輯／刪除 |
| `filament_create/edit_filament.html` | `guide/23/23.css` | 共用耗材清單樣式嘅另一直接使用者，編輯對話框同橋接語意保留 |

`filament_create/step2_type.html` 同 `step2_copy.html` 使用其他樣式，本單元冇改，唔可以因為名稱有 step2 就當已覆蓋。

## 行為邊界

原 CSS 宣告保留，後面加入限定範圍嘅組合規則。隱藏路由、未啟用對話框、登入／外掛狀態、檔案遮罩、選取方格、配件輸入、停用噴嘴及隱藏自訂群仍用原顯示規則；冇新增 opacity、visibility、pointer-events、display:none 或 important 覆蓋。

首頁迷你列保留原項目數目規則，只有完整最近檔案集合改網格。Wiki 輪播寬度同程式計算捲動不變；畫廊圖片選取、尺寸輸入、鍵盤、導覽指示位置同錨點派發保留。配件下拉選單保留錨點及行內顯示擁有權，只限制內部捲動。參數 ID、選擇器、啟用／停用處理，同資料產生嘅色塊、縮圖、百分比、狀態值全部保留。

使用原主題變數同本地字體。新增減少動態規則只處理受影響 CSS 過渡，冇改 JavaScript 動畫，亦唔宣稱佢哋已支援減少動態。

## 來源證據

- 最初確認八個入口、十三個匯入；再發現 `edit_filament.html` 直接匯入 `guide/23/23.css`，正確總數係九個入口、十四個匯入。
- 十三個改動檔案保留基準宣告，新增區塊括號平衡，冇隱藏／透明度／指標事件／important 覆蓋。呢個係來源邊界檢查，唔係完整 CSS 解析器或瀏覽器版面結果。
- `node --test tests/web_resources_accessibility.test.mjs`：三項通過、一項失敗。通過項目涵蓋項目縮放／鍵盤、原生項目操作及焦點、通知嚴重程度／live role／關閉／持久化／堆疊。
- 失敗嘅指引檢查期待 `guide/23/23.js` 有行內 CFEdit 處理器；該檔正規化換行後同基準逐位元組一致，實際用綁定 click 呼叫 `CFEdit(id)`。基準已經唔符合呢個舊期待，冇改產品 JavaScript 或測試去掩飾。
- 冇完整建置、程式／瀏覽器啟動、擷圖、部署或硬件操作。

## 驗收同還原

原生主程式能否到達入口、明暗主題、雙語排版、緊湊設定、比例、焦點、輪播、圖片選取、對話框邊界同取消／復原都待真實內嵌執行驗證。來源檢查唔等於完整重設計或無障礙驗收。迷你列規則、其他耗材建立流程、JavaScript 動畫同原有指引測試不一致仍係跟進邊界。可獨立還原呢個 CSS 單元，保留較早渲染、工具提示、通知同功能修正。

## 縮圖顯示軸修正

最初窄版規則改成橫向縮圖列，但原 `setActiveThumb` 用 `position().top`、`outerHeight()`、`height()` 同 `scrollTop()` 顯示選取縮圖。修正後喺原 max-height 捲動容器保留直向堆疊，JavaScript 不變。

`tests/web_gallery_reveal_axis.test.mjs` 用有限替身執行真實顯示函數，檢查視窗上下方選取，並拒絕不相容橫向 overflow 或列排版。舊 CSS 一項通過、一項失敗；修正後兩項通過。呢個係來源及處理器模型證據，唔係瀏覽器或鍵盤執行結果。較早無障礙測試仍係三項通過、一項基準行內處理器期待失敗。
