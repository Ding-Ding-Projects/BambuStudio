---
translation-of: school-mode.md
source-sha256: 43817633dc51a98ad37ba9c5e4bf50a4e17cc3d3b815e950f6b97a674537c59a
review-status: agent-drafted
---

# 共用顯示模式

建議嘅第 1 版互通記錄放喺 `%LOCALAPPDATA%/DingDing/SharedPresentation/school-mode-v1.json`。同一位 Windows 使用者嘅應用程式共用呢份記錄。呢個係方便切換介面嘅鎖，唔提供安全隔離。刪除記錄會刻意將模式重設為關閉。認證憑據留喺作業系統嘅憑據庫，唔會寫入記錄。

JSON 有且只有六個欄位，編碼後最多 4096 個位元組，唔接受巢狀容器或者重複鍵。

| 欄位 | 類型同限制 |
| --- | --- |
| `version` | 整數，必須係 `1` |
| `enabled` | 布林值 |
| `displayName` | 有效 UTF-8，1 至 128 個位元組，必須有可見內容，唔可以包含控制字元或者強制變更文字方向嘅字元 |
| `revision` | 64 位元無符號修訂號，每次成功寫入後遞增 |
| `credentialGeneration` | 憑據庫嘅不透明代次識別碼，只可以用可顯示嘅 ASCII 字元，最多 128 個位元組；啟用時必填 |
| `updatedAt` | UTC 時間，格式係 `YYYY-MM-DDTHH:MM:SSZ` |

`SchoolStore` 獨佔鎖定旁邊嘅 `school-mode-v1.lock`，重新載入記錄，比對預期修訂號，寫入名稱獨一嘅臨時檔案，再原子取代舊檔。同時寫入嘅另一方會收到 `Conflict`，必須重新載入，唔可以覆寫。停用時要喺持有檔案鎖期間額外核實當前憑據嘅確實代次。轉接器要查詢同驗證 `LocalSecurity::shared_mode_account`，喺驗證前後比對代次，並喺同一項共用記錄交易內協調憑據更換。PIN 同密碼資料只會留喺 `LocalSecurity::Secret`。服務未有通行密鑰登記功能，未接駁真正嘅平台轉接器前唔可以顯示為可用。

`SchoolRuntime` 監察共用資料夾，另設每 500 毫秒輪詢一次嘅後備途徑。即使監察出錯，輪詢仍可偵測記錄建立、刪除、改名同憑據代次變更。管理呢個物件嘅程式碼要如實提供 `native_watch_available()` 同 `polling_available()`，並用目前揀選嘅顯示名稱交代讀取或者損毀狀態。如果名稱不明，就用一般嘅未能使用提示。

記錄確定不存在代表刻意重設，模式會關閉。如果無法讀取或者記錄損毀，就保留上次有效名稱並繼續限制顯示內容。啟動後確認記錄有效或者確定不存在之前，同樣維持限制。唔可以將狀態不明當成模式已關閉。

`effective(base)` 保留原有偏好物件，回傳英文、嚴謹語氣嘅顯示設定，暫停個人用字及驚喜內容。離開模式會恢復一直保留嘅偏好。`available(capability)` 統一決定控制元件、搜尋、指令面板同直接啟用路徑可唔可以用。應用程式要重建呢啲介面，單單隱藏控制元件並唔足夠。模式控制元件只用 `record().display_name`，包括無障礙名稱、搜尋文字同描述。各項專注輔助仍可獨立使用。
