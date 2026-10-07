---
translation-of: readable-dates.md
source-sha256: 1b516af2b3429f47f533968b6fcc07b7c5bc763f292f05e6ed548f509330646f
review-status: agent-drafted
---

> 英文原文：[Readable dates](readable-dates.md)

# 易讀日期

面向使用者嘅英文日期用完整月份名稱，例如 `29 September
2026`。廣東話用 `2026年9月29日`；雙語模式同時顯示兩者。適用範圍包括建置資訊、版本歷史、最近專案、專案同設定檔快照、通知、工作區議程同限期、印表機媒體，以及套件網頁同文件表面。

UTC 時間戳只喺呈現時轉換一次至觀看者本地時間。只有日期嘅值，喺所有時區保持同一個曆日。機器儲存、網絡訊息、排序鍵、檔名、紀錄同日期編輯器輸入語法，保留原有格式。最近專案繼續遵從使用者 12 小時時鐘設定。

啟動畫面用二進位檔案編譯時嘅 UTC 時間戳報告 **Built**。建置時間唔能夠證明版本發佈時間。啟動唔會等網絡，亦唔會換成無關最新版本日期。日後 **Released** 標籤需要目前執行版本嘅已驗證發佈中繼資料。

原生呈現共用 `src/slic3r/GUI/HumanDate.hpp`。套件網頁共用 `resources/web/include/human-date.js`；逐位元組一致嘅文件副本位於 `ui-md3/site/human-date.js`。自動比較防止兩份偏離。無效日期唔會產生捏造替代值。

針對性檢查位於 `ui-md3/tests/human-date.test.mjs`，涵蓋三種語言模式、閏日、無效值、純日期穩定性、UTC 轉觀看者本地時間，以及 Toronto 夏令時間轉換。原生建置同視覺驗證仍然必要，先可以確認實際呈現佈局。
