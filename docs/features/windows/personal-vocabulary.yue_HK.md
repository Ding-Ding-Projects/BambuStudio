---
translation-of: personal-vocabulary.md
source-sha256: 7fda420aacc18e221da22803e91685fa11beb504f516a621ff6cacab20b092ac
review-status: agent-drafted
---

# 個人詞彙

Preferences > Appearance > Personal vocabulary 載入本機版本 1 JSON 檔。同一控制項亦可替換已載入檔案。Clear personal vocabulary 唔使重啟就恢復原有文字。無效輸入保留目前對應。

```json
{"schemaVersion": 1, "entries": {"Open": "Inspect"}}
```

載入器最多接受 1 MiB、2,048 個項目，以及每鍵／值 512 UTF-8 位元組。鍵同值必須係非空文字。重複鍵、額外根欄位、不支援版本、控制字元、方向控制字元、巢狀值，同不安全物件鍵名都會拒絕。匹配區分大小寫，最長來源優先，按單字邊界，唔遞迴。空項目物件有效。

只有本地化邊界登記嘅已翻譯介面文案可參與。最終共用標籤、按鈕、原生選單同 combo 彈出顯示配接器套用對應；可編輯輸入同來源模型值永不重寫。URL、路徑同格式範本保留原文。偏好設定搜尋讀取原有未換行標籤。畫布顯示配接器使用同一介面。

持久化使用作業系統本機程式資料目錄下嘅 `private-display/vocabulary.json`，唔屬於同步偏好。每次替換使用唯一、獨佔建立嘅同層暫存檔，再以 Windows 原子重新命名。並行寫入者唔能重新命名對方待處理內容，清除亦永不刪除另一操作待處理檔案。唔保留所選輸入路徑。承載內容、對應值或者所選檔名都唔記錄或經網絡發送。設定檔封存排除私有快取，包括自訂資料夾包含快取嘅情況。共用標籤保留原生原文同全部原有 getter 值，只喺繪製同量度時替換。按鈕同選單模型亦保留原值。因此記錄、操作比較同排列收據繼續取得原文。

## 驗證

呢次變更冇本機建置、測試或者程式執行。喺已設定託管 Windows 建置，建立 `personal_vocabulary_tests` 同 `personal_vocabulary_probe`，再依次執行測試同探測階段：`load-first`、`restore-first`、`invalid`、`replace`、`restore-second`、`clear`、`restore-empty`。每次探測都必須係新程序。探測只喺隔離程式資料命名空間使用合成中性文字，只輸出通過／失敗。亦須以獨立程序同時執行 `race-a` 同 `race-b`，等兩者完成，再執行 `verify-race` 同 `clear`。

原生 UI 仍須託管互動證據，涵蓋載入、套用文字、重啟、替換、清除同無效輸入，以及匯出／記錄保留原文。測試正常／最低尺寸、英文／粵語／雙語、淺深色同 100/125/150/200% 比例。擁有者私有 JSON 唔可以複製到公開託管輸入、記錄或者擷取；其私有端對端驗證獨立進行，喺嗰個環境仍不可用。源碼覆蓋唔聲稱全部自訂繪製表面已驗證。

`personal_vocabulary_native_contract` 源碼檢查拒絕將對應文字寫入原生標籤儲存，並測試已審查錯誤 setter 變異。唔可以取代原生互動：載入合成對應後，驗證 `Label::GetLabel()`、`GetLabelText()` 同 `GetUnwrappedLabel()` 喺繪製、替換同清除前後保留原值。喺託管程式檢查校準標題記錄、噴嘴架操作比較同排列收據標籤，與原值一致。
