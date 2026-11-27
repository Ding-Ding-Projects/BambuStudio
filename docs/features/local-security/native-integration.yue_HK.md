---
translation-of: native-integration.md
source-sha256: 9512c7b7654a25534e9c5a544b8971ac7e88770a6ebca8e7a8dc281bd25f9dce
review-status: agent-drafted
---

# 原生本機安全服務整合

服務元件同四個原生介面元件已有實作。原生應用程式未有編譯或者操作呢批新元件。以下列出整合約定同未完成工作，唔代表已發佈。

## 物件生命週期同儲存接線

產品本機服務共用一個 `make_application_vault(data_dir)` 實例。非機密嘅隨機實例標記存喺 `local_security/instance-v1`，重啟時重用。刪除應用程式資料夾會建立新命名空間，新實例無法存取舊有系統憑據記錄；如有需要，可以明確經作業系統憑據管理移除。絕對唔好由顯示名稱推導資料目錄或者命名空間。

為 `shared_mode_account` 另外建立 `make_os_vault()`。共用顯示模式憑據刻意放喺產品本機命名空間之外。共用模式負責方喺同一憑據庫嘅 `shared_mode_attempts_account` 保存並執行 `AttemptBudget` 限制。共用設定記錄只儲存 `Credentials` 回傳嘅已設定旗標同代次。

每次執行應用程式只保留一個 `IdentityHistory`、`AuthenticatorStore` 同 `SupportTickets`。建構方式係 `IdentityHistory(data_dir, *local_vault)` 同 `SupportTickets(*local_vault, data_dir)`。第一次要明確登記歷史憑據，先可以記錄身分變更。`AuthenticatorStore(*local_vault, record_mutation)` 必須接駁真正同步歷史接收端，呼叫 `IdentityHistory::append` 並傳回失敗，唔可以傳入空白或無作用嘅接收端。

動作對應如下：`AuthenticatorAdded` 對應 `HistoryAction::Created`；`AuthenticatorChanged`、`AuthenticatorReordered` 同 `DisplayNameChanged` 對應 `Renamed`；`AuthenticatorRemoved` 對應 `Removed`；`AuthenticatorRestored` 對應 `Restored`。經驗證嘅分派器只可以為確認屬於該儲存區嘅身分呼叫 `AuthenticatorStore::restore_entry(identity, snapshot)`。要驗證完整歷史項目，保留原有身分，再用同一歷史接收端記錄還原。記錄經歷所有變更仍然用原本產生嘅 32 位十六進制 ID。`LockCreated` 只記錄五個位元組嘅規則同有效時間設定，唔包含憑據。每個顯示元件都要有共用元素登記表維護嘅獨立穩定 ID。

每個元件身分配一個 `ElementLock`，用同一本機憑據庫同真正歷史接收端，喺所有操作途徑都沿用同一實例。滑鼠、鍵盤、觸控、指令面板、快捷鍵、拖放同程式直接啟用之前，必須呼叫 `allows_action(steady_clock::now())`。回傳 false 就開 `LockWizard`，唔執行原操作。解鎖成功亦唔會自動重播原操作，使用者要再啟用一次。關閉所屬介面時呼叫 `leave_surface()`，明確重新鎖定用 `relock()`。

## 原生入口

所有入口都喺 `Slic3r::GUI::LocalSecurityUI`。

| 入口 | 主程式責任 |
| --- | --- |
| `AuthenticatorPanel(parent, shared_store, hooks)` | 登記一般分頁式目的地同指令面板入口。 |
| `LockWizard::Open(anchor, shared_lock, target_name, recovery_folder, hooks)` | 提供確實錨點、目標同真正應用程式資料夾，攔截每個受保護操作。 |
| `SupportTicketsPanel(parent, shared_store, data_dir, hooks)` | 將說明、鎖定設定同忘記答案入口接到呢個目的地。 |
| `IdentityHistoryPanel(parent, shared_history, hooks, restore_dispatcher)` | 登記受保護歷史同負責按類型還原實際狀態嘅元件。 |

`Hooks::text` 處理現用語言同語氣。`factual_text` 處理語言但唔改語氣，負責支援說明。`notify` 將錯誤送去現有非阻塞通知服務。`register_surface` 接駁共用逐元素外觀同內容選單。`register_sensitive` 將目標元件排除喺歷史、日誌、診斷、分析同截圖之外。以上回呼必須提供。`export_text` 接駁知道格式嘅既有匯出流程；冇接駁就清楚標示並停用匯出。`open_support` 開啟實際已登記嘅支援頁。

`record_label`、`record_tooltip` 同 `record_name` 保留原始公開文字，供即時切換語言。靜態標籤同動作要保留來源，輸入值同即時產生嘅事實唔會經呢條路徑改寫。動態狀態由元件自己管理。`record_factual_label` 係歷史、示範鎖同支援靜態說明必備，只可以換語言，唔改語氣。遮罩欄位只登記無障礙名稱，唔登記代替用嘅值。

未接駁共用確認服務前，`confirm_retention(anchor, consequence, revisions, callback)` 係可選。接駁後必須等兩個獨立確認都完成、滑桿推盡先傳遞真正原生 `SuperConfirm::State` 嘅副本，唔可以自己砌出旗標。使用端建立經身分驗證嘅預覽，綁定當前最新修訂同確實選取修訂，顯示 ID 同實際後果，使用確認狀態前再核實。冇接線就要明確停用移除。移除對現有內容嘅解密存取後，中繼資料仍會保留。備份、憑據庫副本同檔案系統日誌唔受影響，唔保證法證級清除。原生管理器會顯示容量上限，並標記已移除解密權限但仍保留嘅中繼資料。第 1 版內容仍然可讀，唔可以用第 2 版移除操作處理。

原生介面沿用現有 `SearchField`、`TextInput`、`Button`、`Label` 同對話框外框。四個本機搜尋各有自己嘅錨定正規表達式編輯器。實作期間冇可呼叫嘅設計建立或匯出途徑，亦冇已驗證設計交接。沿用元件只屬原始碼證據，未證明實際版面或設計一致。

喺程式庫 CMake 加 `add_subdirectory(LocalSecurity)`。建立 `libslic3r_gui` 後，喺 GUI CMake 加 `add_subdirectory(GUI/LocalSecurity)`。如果已有 `libgit2::libgit2package` 目標，程式庫子目錄亦會定義 `libslic3r_identity_history`，組合時要連結該目標。測試目錄加入 `add_subdirectory(local_security)`。

## 已實作行為同界限

驗證器喺原生憑據庫最多保留 32 項，配對確認後先登記，保留簽發者、帳戶同群組資料，可調整排序，顯示當前同下一個驗證碼。只會喺使用者明確操作時複製驗證碼，匯出只包括已遮蔽中繼資料。URI 會保留摘要演算法、位數同期長；手動登記介面會講明固定使用 SHA-1、六位數、30 秒。QR 匯入未能用時會明確標示。

鎖定精靈提供六項規則、三個持續時間、明確嘅示範鎖說明、確實復原資料夾、順序驗證，以及接駁同一後端嘅 PIN 鍵盤同手動輸入。精靈係非模態，初次開啟時錨定並限制喺可視區，未能跟隨移動錨點，亦未有逐級解鎖介面。搜尋只查設定名稱，唔睇敏感值，亦唔會隱藏復原說明。

支援工單係本機模擬功能。模型可以建立、列出、推進同匯出工單，最多保留八筆，每個描述最多 160 個 UTF-8 位元組。記錄加密儲存喺 `local_security/support-tickets-v1.enc`，憑據庫只儲存密鑰。介面可顯示同複製確實應用程式資料夾，要求平台檔案管理器開啟，但絕不刪除資料。支援服務同呢啲元件冇網絡程式碼。

## 驗證同未完成工作

Windows C++17/OpenSSL 核心喺 MSVC 通過 **223 項行為斷言**；之前 GCC 用 `-Wall -Wextra -Werror` 通過 219 項。覆蓋標準向量、憑據同鎖、驗證器持久變更、後端因子驗證、一次性密碼重播拒絕、加密工單生命週期、真正原生憑據庫來回讀寫、重啟與重設標記、零大小及最大 AES-GCM 快照、歷史相容元素 ID 同驗證器還原。

隔離變異測試喺無法驗證時會停止。GCC 連續三次有內部編譯器錯誤。改用已驗證 MSVC 19.51 後，十二個刻意破壞嘅副本逐一出現預期測試失敗，原版同還原後版本都通過。測試使用現成 OpenSSL 標頭同 x64 DLL，檢查全部必需 C ABI 匯出後先臨時產生 MSVC 匯入程式庫。結果只驗證服務，未驗證最終封裝依賴。已設定嘅 MSVC 開發者命令列可執行：

```sh
python tests/local_security/negative_regression.py --msvc --compiler cl --openssl-include <include-directory> --openssl-dll <matching-x64-crypto-dll>
```

亦可用 `--openssl-library` 指定現有相容匯入程式庫；測試唔會安裝或者取代共用依賴。

獨立 libgit2 歷史測試用指定 1.9.3 建置配方同穩定 MSVC 19.51 編譯，通過 **118 項身分歷史檢查**。三項隔離保留規則變異均按預期失敗，還原後 118 項再度通過。QR 矩陣用公開合成資料，經獨立 ZXing 解碼同精確參數比對通過。原生面板仍未編譯，未有實際互動、無障礙、語言或者版面截圖。

完成前仍要接駁所有主介面操作、即時語言與語氣、真正顯示嘅 QR 驗證、封裝圖片／剪貼簿／相機匯入、手動登記參數、實際歷史管理器同保留確認、原子協調實際狀態與歷史、驗證持久嘗試上限同具名互斥鎖、逐級解鎖介面、動態錨點追蹤、完整批量操作及篩選後多格式匯出、逐元素內容選單，仲要用真正建置驗證主題、語言、視窗尺寸同縮放。

相關文章：[服務總覽](README.md)、[加密身分歷史](identity-history.yue_HK.md)、[配對同匯入封裝](qr-pairing.yue_HK.md)。
