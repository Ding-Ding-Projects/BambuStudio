---
translation-of: README.md
source-sha256: f37822a1210fae9ba32f6c7f6e2ef885729210bffc9339cb5b7ec4d38982f19f
review-status: agent-drafted
---

> 英文原文：[Design system](README.md)

# 設計系統

呢個分類記錄原生 wxWidgets/OpenGL 應用程式點樣使用內置嘅 Material Design 3 設計系統。

- [目前原生介面審查](native-interface-audit-2026-10-02.md)：目前選單、動態、個人詞彙及切片操作要求，列明待完成嘅託管驗證同私隱邊界。

- [內置嘅 Material Design 3 設計系統](md3-design-system.md)：設計令牌嘅唯一來源、由零開始遷移嘅顏色、字型同尺寸、
  情境配色、字體、出錯情況，同對照審計嘅結果。
- [MD3 對照登記表](md3-parity-register.md)：逐個元素記錄符合程度嘅標準登記表，同推動結構遷移嘅分批計劃。完成、
  偏離同未處理嘅數目以登記表本身為準，唔好睇其他地方嘅舊快照。
- [物件操作工具列 SVG 圖示完成度](gizmo-rail-svg-icons-completion.md)：為餘下嘅組合圖示加上 34 個用 MD3 設計令牌
  嘅覆蓋圖；要有原生程式執行時嘅證據，對照表嗰行先可以由部分完成改做完成。

- [每個元素檢查期間加入嘅套件組件](kit-widgets-2026-09.md)：LabeledRadioButton 同 RadioGroup、TextArea、ListBox、
  Button::SetIconBitmap，同預設就係 Material 嘅按鈕。
- [執行時版面探針](layout-probe.md)：預設關閉嘅 NDJSON 版面掃描器，自動搵出擠到冇位嘅排版行、零大小嘅控件同被裁剪嘅
  標籤，連同佢嘅報告閱讀工具。
- [版面裁剪清單](clipping-inventory.md)：每個搵到嘅裁剪缺陷，連同元組、原因、修正提交同前後擷圖；有自動檢查，冇證據嘅
  行唔可以話已經驗證。
- [StaticBox 卡片嘅主題表面顏色](themed-surface-colors.md)：卡片點樣攞到填色、點解 `SetBackgroundColorNormal()` 可能
  靜靜雞冇效，同深色模式入面淺色打印板後面，喺建構時已經過時嘅視窗背景。
- [生成嘅視覺展示](generated-visual-showcase.md)：互動程式、GitHub Pages 首頁同社交預覽共用嘅圖片組，包括載入、無障礙、
  部署同驗證行為。
- [右鍵選單](context-menus.md)：每個右鍵選單都係 Material 選單，包括文字欄由滑鼠同鍵盤打開嘅編輯選單、可以複製嘅標籤同
  網頁，同點樣檢查發佈套件有冇系統選單。
- [工具提示](tooltips.md)：每個工具提示都係 Material 純文字工具提示，兩種主題都係，同點樣檢查發佈套件有冇系統工具提示。
- [對話框同揀選器](dialogs-and-pickers.md)：取代 wxWidgets 內置提示、揀選器、忙碌通知同顏色對話框嘅 Material 對話框、最近使用嘅顏色，同邊啲保留原生。
- [套件上嘅原生控件](native-controls.md)：停用咗嘅掣嘅提示、網頁嘅通知橫額，同工作區面板嘅分頁、表格、待辦清單同日曆嘅套件替代品、所有表格共用嘅 Material 樣式，同埋每個會捲動嘅頁面、面板、清單同表格嘅套件捲動列。

## Studio Atlas 實作記錄

以下文章記錄限定範圍嘅來源改動及驗證限制，唔代表原生畫面或整個應用程式重設計已驗收。

- [共用控件](studio-atlas-shared-controls.yue_HK.md)：按鈕、卡片、搜尋、選單結構同呼叫者缺口。
- [欄位同預設](studio-atlas-fields-and-presets.yue_HK.md)：量度幾何、驗證顏色及預設控制。
- [Prepare 檢查器同清單](prepare-inspector-atlas.yue_HK.md)：原生檢查列、章節及選取表面。
- [渲染器畫面](renderer-atlas.yue_HK.md)：工具列、提示、時間軸及 Preview 圖例繪畫。
- [原生監察](monitor-atlas.yue_HK.md)：遙測層次、打印機選取、相機頁尾及 DPI 修正。
- [裝置彈出介面](device-popups-atlas.yue_HK.md)：相機、風扇、映射、材料及 AMS 設定。
- [項目、偏好設定同設定流程](native-preferences-setup-atlas.yue_HK.md)：原生頁面、卡片生命週期及巢狀搜尋。
- [工作區](workspace-atlas.yue_HK.md)：原有五頁同內容觸發重新排版。
- [設定索引](setup-index-atlas.yue_HK.md)：保留路由嘅精靈流程繪畫。
- [校準子頁面](calibration-children-atlas.yue_HK.md)：步驟、預設、進行中及結果頁。
- [閱讀器同覆蓋介面](overlays-atlas.yue_HK.md)：八個輔助原生介面同明確子頁面邊界。
- [即時通知](live-notifications-atlas.yue_HK.md)：渲染器通知卡同原操作目標。
- [內嵌配色](embedded-studio-atlas.yue_HK.md)：產品樣式顏色、焦點及減少動態規則。
- [內嵌頁面組合](embedded-composition-atlas.yue_HK.md)：九個入口、十四個匯入及來源驗證限制。

## 設計來源

倉庫入面嘅標準設計來源係 [`ui-md3/design-system/`](../../../ui-md3/design-system/)。嗰度嘅設計令牌數值同
`src/slic3r/GUI/Widgets/MD3Tokens.hpp` 完全一致；C++ 程式碼以呢個標頭檔做原生嘅唯一來源。

## Postman 集合

不適用。設計系統係桌面應用程式編譯時嘅設計令牌同字型層，冇任何 HTTP 或 API 介面，所以呢個分類冇 Postman 集合。
