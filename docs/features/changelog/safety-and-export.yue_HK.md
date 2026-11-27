---
translation-of: safety-and-export.md
source-sha256: 727fb475beff03720efe9172780953955d708ff3e2539ad4ad953333ec601faa
review-status: agent-drafted
---

# 更新紀錄完整度

檢視器保留日期月曆、手動輸入日期嘅驗證、可以合併使用嘅文字同正規表達式搜尋、跟顯示語言複製，以及指向完整提交嘅連結。匯出會將同一份篩選後嘅 Markdown 交畀共用匯出對話框，當中包括日期範圍、搜尋條件、版本同變更數量、日期同完整提交參照。各格式會遺失邊啲資料，由共用匯出工具解釋。

匯出工具會喺本機核實每個引用嘅提交，並透過現有發佈工具逐頁讀取已發佈版本。如果版本服務出錯，時效檢查指令就會報錯，唔會喺無法查證時當成最新。

建置整合必須先擷取發佈標籤，再喺封裝前執行 `node scripts/changelog/export-app-changelog.mjs --check`，需要已登入嘅 `gh`。用 `node scripts/changelog/export-app-changelog.mjs` 更新資料。`--offline --check` 只會核對已提交嘅網站快照，唔能夠確認涵蓋所有現時已發佈版本。建置檔案由整合工作負責，呢次冇更改。

**驗證狀態**　Node 測試涵蓋格式錯誤嘅日期、無效或者不存在嘅提交、冇變更項目嘅真實版本、來源不可用時停止嘅接線，以及檢視器同共用匯出工具嘅連接。實際程式內嘅月曆、複製、匯出同編輯器互動，仍要用 wxWidgets 建置驗證。
