---
translation-of: appearance-property-pages-atlas.md
source-sha256: b1c34c964c5aed925d90a2aa7a53cfffd5d0d7d0a37caa5a72b528e2abba73af
review-status: agent-drafted
---

> 英文原文：[Studio Atlas appearance property pages](appearance-property-pages-atlas.md)

# Studio Atlas 外觀屬性分頁

## 範圍同介面組合

呢個只涉及源碼嘅單元，修改 `Appearance/AppearanceEditorPopover.cpp` 入面四個屬性分頁建構函式，由 `8fb54ada88eb9d92afc13b0712d412ba3bb55aa7` 開始。共用目錄、確認控制項、正則運算同匯出對話框源碼都冇改。

- 字體：大小、粗幼、字距同行高，改用持續可見而可換行嘅標籤，下面係按實際尺寸量度嘅數值／重設列。控制項最低尺寸仍然有效，重設目標保留。斜體、底線同刪除線各自將核取方塊、標籤同重設保持成一組，整組換行。
- 顏色：文字、背景、反白同邊框，使用相同上下排列標籤及色板／重設結構。原有顏色揀選器、已選值、對比計算同重設路徑保持不變。
- 形狀同間距：邊框闊度、圓角、內距同外距，使用上下排列標籤及量度後嘅數值／重設列。數值上限依次仍然係 12、64、64 同 64。
- 預設：目前預設名稱由省略改為換行。套用／另存／刪除同匯出／匯入各自有可換行操作組，分頁排列器提供可用闊度，換行之間保留空隙。原有次序、啟用條件同回呼不變。
- 現有解說備註改用 body-13 輔助文字。冇新增可見文案或者控制項。字體家族清單／搜尋及其原有預覽保持不變。

較早建立嘅獨立捲動分頁外框，仍然負責溢出內容。呢次源碼工作唔代表每種自訂字體、雙語文字、即時密度變更或者極窄視口組合，都已經有正確計算高度同捲動範圍。

## 保留行為同驗證

`ui-md3/tests/appearance-property-pages-atlas.test.mjs` 由起始版本記錄實際子控制項事件綁定呼叫本體，以及十個登錄／焦點／輔助方法嘅指紋。亦記錄屬性宣告、數值範圍／預設值同顏色屬性表。記憶體內刻意變異會令字體粗幼回呼寫入字體大小；回呼保留檢查必須拒絕佢。針對幾何嘅檢查要求上下排列標籤、重設列保留、完整裝飾控制組、可擴展預設操作組，以及目前名稱換行。呢啲係源碼合約，唔係原生執行測試。

```sh
node --test ui-md3/tests/appearance-property-pages-atlas.test.mjs ui-md3/tests/appearance-decimal-field.test.mjs
```

**18 項檢查通過**，包括三項原有小數欄位檢查。冇新增翻譯鍵。`git diff --check` 同新增內容公開邊界掃描通過。未有執行完整建置、啟動程式、擷取畫面或者執行安裝程式。原生編譯、可見焦點、文字容納、對比度同執行行為仍未驗證。

## 巢狀後續工作

- 喺實際建置程式操作每個外觀屬性、重設、字體預覽、預設保護、另存、刪除、匯入同匯出；涵蓋長標籤同目前名稱、英文／粵語／雙語、淺色／深色、舒適／緊湊密度、自訂字體及 100/125/150/200% 縮放。驗證登錄驅動變更後嘅分頁捲動範圍，以及換行唔會移走焦點或者丟失操作。
- 正則診斷由 `RegexBuilderPopup.cpp` 嘅 `build`、`buildReference`、`addSection`、`fitPopup` 同 `evaluate` 分別負責。狀態／參考狀態標籤、固定闊度樣本文字／結果編輯器、body-11 等寬結果文字，同產生嘅匹配／診斷內容，唔屬於呢個單元。
- 匯出詳情由 `ExportDialog.cpp` 嘅 `create_ui`、`update_format_details`、`update_archive_panel`、`update_hints` 同 `set_status` 分別負責。保真度／資料損失／結構描述、封裝備註、7-Zip 可用狀態、成本／加密備註、狀態、長核取方塊文案同操作組，仍然係獨立工作。現有序列化、密碼同執行路徑冇改。
