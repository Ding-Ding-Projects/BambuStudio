---
translation-of: safety-and-export.md
source-sha256: bcfa1c5c0031e70560f8cba0ec431478fd336b443496332ddff3c019d6b60e02
review-status: agent-drafted
---

# 通知記錄

刪除之前，先根據當前選取同篩選條件檢視仍然存在嘅 ID，確認回呼只會處理當時嗰組 ID。之後改篩選、改選取或者變更保留記錄，都唔會加入新目標。完成時會分開報告已刪記錄數量，同埋檢視過但已不存在嘅數量。無關項目會繼續保持選取。所有刪除都必須經過共用雙重確認。

匯出會固定當時選取而又符合篩選嘅記錄；如果冇揀到任何符合條件嘅項目，就匯出全部符合條件嘅記錄。共用匯出對話框負責格式選擇、資料損失說明、封存設定、目的地處理同外部編輯器交接。每行保留 ID、時間戳、級別數字同名稱、通知類型、標題、全文、關閉狀態同時間、已讀狀態、動作、篩選條件說明同記錄總數。為免遺失 64 位元無符號整數精度，ID 用十進制字串保存。

**驗證狀態**　獨立 C++17 模型測試包括覆核後改選取、保留機制移除已覆核記錄、不明 ID，以及完整匯出篩選結果。Node 使用端測試會刻意改用後來嘅選取，確認測試必定失敗。原生對話框互動、翻譯、縮放與版面，以及逐次點擊截圖，仍有待完整 wxWidgets 建置驗證。

執行 `pwsh -File tests/notification_history/run-native.ps1` 可重做獨立原生測試；執行 `node --test tests/notification_history/consumer_contract.test.mjs tests/changelog_completion/exporter.test.mjs` 可重做七項來源同匯出測試。呢啲測試唔會編譯或者操作原生對話框。
