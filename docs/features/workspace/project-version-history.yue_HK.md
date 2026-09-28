---
translation-of: project-version-history.md
source-sha256: 6f84b9da554756e909ed09fbd055aad459647a3002dfbc892fa4972ebf857d05
review-status: agent-drafted
---

> 英文原文：[Project version history](project-version-history.md)

# 項目版本歷史

每個項目嘅本地、Git 支援版本歷史。一個普通儲存嘅 `.3mf` 包含佢嘅模型加上一個版本化清單同 libgit2 物件包。應用程式仲會喺側面儲存一個隔離嘅光禿倉庫作為工作快取。冇野會同步或推送到任何地方；用戶個人資料夾度冇出現過 `.git`。

## 行為

- **擷取**：編輯同設定變更會排計一個防抖快照 (`Plater::priv::schedule_project_history_capture`)；手動儲存會擷取剛剛儲存嘅存檔。自動擷取使用決定性匯出 (`SaveStrategy::Deterministic`)，所以未改動嘅項目會去重到同一個 blob。手動儲存會首先匯出一個無歷史嘅模型快照、提交咗嗰個快照、然後附加可到達嘅歷史物件到已儲存嘅存檔。
- **儲存** (`src/libslic3r/ProjectHistoryManager.{hpp,cpp}`)：libgit2 v1.9.3 光禿倉庫喺 `<data_dir>/project_history/v1/<sha256(project identity)>` 底下，每提交一個完整 `.3mf` blob，由一個單一序列化工作線程寫入。未命名嘅工作階段會得到一個按工作階段身份，喺另儲存時會遷移。
- **便攜存檔**：`Metadata/bambu_project_history.json` 識別格式版本、穩定文件 ID、活躍頭部、保留血統、包路徑、位元組長度同 SHA-256。`Metadata/bambu_project_history.pack` 儲存可到達嘅 libgit2 物件。開啟時，應用程式驗證有界項目、包摘要、有界展開物件大小、提交圖形、物件類型同快照樹，然後先將歷史匯入本地快取。損壞嘅歷史載體會被拒絕，而有效嘅目前模型幾何仲然會載入。另儲存時會分配一個新文件 ID 同繼承已驗證嘅圖形。持有同一個文件 ID 嘅第二個路徑喺儲存時會收到一個新 ID，而原始嘅仲然存在，所以複製嘅項目可以獨立發散。如果原始路徑已經消失，ID 會跟隨已移動嘅檔案。本地擁有者標記儲存喺 `<data_dir>/project_history/document-identities` 底下。
- **儲存發佈**：完成嘅模型同歷史被組合喺目的地旁邊，並在原子替換之前重新開啟以進行驗證。失敗嘅原子替換會保留前面嘅存檔、已驗證嘅歷史承載分階段存檔同無歷史嘅待批快照以供復原。模型快照唔包含嵌入式歷史，避免遞迴增長。便攜包限制喺 512 MiB，清單限制喺 16 KiB。在 libgit2 攝入包之前，流式預檢允許最多 100,000 個物件、每個展開物件或增量結果 2 GiB（因此每個快照）同總共 8 GiB 展開物件位元組。佢使用一個固定 64 KiB 解膨脹緩衝區。超出限制嘅歷史會阻止便攜發佈，直到透過刻意保留操作進行減少；前面嘅存檔保持完整。
- **目前版本匯出**：檔案 > 匯出 > 只匯出目前版本為 3MF 會寫入目前模型同設定，唔包含便攜歷史項目。佢會先沖洗待批擷取、驗證分階段 ZIP 同原子發佈佢。失敗時，前面嘅目的地同待批匯出保持可用。喺同一個應用程式設定檔度重新開啟已儲存嘅存檔時，當佢由嵌入式頭部下降時，會保留一個更新嘅本地自動儲存為活躍版本；新設定檔會啟用嵌入式頭部。
- **瀏覽/復原**：**檔案 ▸ 版本歷史…** 同頂部欄 `main •` 歷史晶片開啟 MD3 `ProjectHistoryDialog`（提交清單附帶訊息、時間、大小）。上面嘅 `SearchField` 按提交 id、訊息或時間戳過濾版本──預設為純文字，附帶共用嘅 `.*` 正規表達式切換同調整構建器；狀態列會報告「N 之 M 版本與搜尋相符」，而選擇/復原總是透過過濾視圖映射，所以復原嘅提交正係突顯嘅列。復原會將所選版本具體化為臨時 `.3mf` 同交換活躍文件 (`Plater::restore_project_history_snapshot`)，附帶一個回滾存檔保留到交換成功；磁碟上嘅原始檔案永遠唔會被復原原始程式碼覆蓋。

- **崩潰備份儲存**：當應用程式啟動並找到未儲存嘅崩潰備份時，嗰個備份會被提交到歷史**之前**「復原你嘅最後未儲存項目？」提示出現 (`Plater::priv::preserve_unsaved_backup_in_history`)。拒絕提示會刪除備份目錄，所以提交必須先進行──否則「取消」係唯一複本未儲存工作消失嘅時刻。吐司會確認工作已儲存同仲然可從「版本歷史」復原。快照喺真實 `.3mf` 檔案名度分階段（備份檔案按路徑規則實際命名為 `.3mf`，冇副檔名，引擎同時驗證身份同快照嘅副檔名）。身份係已儲存項目路徑，當備份有一個時，否則係編碼喺備份工作階段令牌標記度嘅未命名身份，所以復原嘅工作會重新加入已崩潰工作階段嘅歷史而唔係開始一個孤立嘅。

## 配置

無需配置；歷史對已儲存項目總係開啟。儲存位置喺應用程式個人資料目錄旁邊（見上方），永遠唔係喺用戶項目資料夾度。

## 故障模式

- **快照/提交故障** → 耐久故障提示條，附帶「重試」超連結 (`push_project_history_failure_notification`)，重複故障間去重；失敗嘅快照以清單形式儲存同喺重新啟動時重新採納 (`adopt_orphaned_project_history_failures`)。
- **復原目的地喺受管根度** → 被後端拒絕（防止自我損壞）。
- **並行應用程式例項** → 按例項分階段目錄避免串擾；工作者序列化倉庫存取。

## 安全考慮

- 倉庫係本地光禿倉庫；冇遠端被配置同後端中冇網絡 I/O 存在。
- 項目身份雜湊（正規化路徑嘅 SHA-256）保持倉庫目錄名度冇用戶路徑內容。
- 嵌入式包係根據展開位元組配額檢查，在臨時驗證儲存或持續快取匯入佢哋之前。匯入重複嗰個檢查喺檢查之後重新讀取嘅位元組，所以存檔喺嗰啲步驟之間改變時唔能繞過配額。

## 驗證

- **便攜存檔測試 (2026-09-26)**：`project_history_tests.exe` 通過咗 13 個測試案例同 311 個斷言。Unicode 路徑、發散複製同更新嘅本地提示回歸最初針對未完成實現執行同喺預期斷言處失敗，然後喺對應修正之後通過。套件仲檢查咗一個竄改包、遍歷清單、存檔儲存後拒絕發佈同已移動歷史匯入同繼承血統。`Plater.cpp` 編譯為一個選定嘅發布源檔案。完成嘅 GUI 連結同活躍儲存/載入互動對嗰個變更仲然未驗證。

- **崩潰備份儲存、已驗證活躍端對端** (2026-07-27) 喺真實構建二進制上透過 `.claude/skills/run-bambustudio/`：
  1. 載入 `cube.stl`，等待備份 `.3mf` 寫入（8662 位元組），然後硬殺死進程──一個真實崩潰遺留、唔係合成目錄。
  2. 移除而家已過時嘅 `lock.txt`，冇佢復原提示並唔會出現。
     **噉陣間記錄嘅原因係錯誤嘅**──見下方「為乜嘢一個過時鎖抑制咗復原」以查看實際測量係咩。
  3. 指向 `app/last_backup_path` 向嗰個目錄同重新啟動。[復原提示](../../screenshots/version-history/crash-restore-prompt.png) 出現咗。
  4. 按咗**取消**──執行 `remove_all` 喺備份目錄嘅分支。
  - **結果**：備份目錄消失咗，同提交 `82f6cd1 "Recovered unsaved project"` 喺項目歷史倉庫中存活，攜帶 `project.3mf` **正係 8662 位元組**──取消銷毀嘅備份。呢個係嗰個功能嘅全部重點同佢成立。
- 復原路徑記錄一個 `restore check: last_backup_dir=... has_restore_data=...` 列，因為復原喺拒絕時係無聲嘅，「提示永遠冇出現」之外係無法診斷嘅。

### 為乜嘢一個過時鎖抑制咗復原（測量 2026-07-28）

上方早啲嘅筆記將 `has_restore_data()` 嘅 `catch (...)` 歸咎。嗰個係一個猜測，同探測精確嘅 Win32 序列 `get_process_name()` 執行顯示佢唔係發生嘅情況：

| 探測 | 測量結果 |
| --- | --- |
| `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, …, dead_pid)` | 返回 **`NULL`**、`GetLastError() = 87` |
| 嗰個係 `INVALID_HANDLE_VALUE` 嗎？ | **否**──嗰個哨兵係 `-1` 同永遠唔係喺呢度返回 |
| `GetModuleFileNameExA(NULL, …)` | 失敗（`err = 6`）、讓緩衝區空白 |
| `CloseHandle(NULL)` | 失敗（`err = 6`）──一個**無效句柄關閉** |
| `GetModuleFileNameExA` 喺一個*活躍*進程上、限制存取只 | **成功**──所以活躍例項偵測確實運作 |

一個死嘅 pid 因此產生了一個空白名稱、同執行二進制嘅比較唔符、同 `catch` 永遠冇到達。三個真實缺陷喺嗰個位置被發現代替，同全部三個都被修正：

1. **錯誤嘅故障哨兵** (`src/libslic3r/utils.cpp`)。`INVALID_HANDLE_VALUE` 防護錯過咗 `OpenProcess` 嘅實際 `NULL`，所以一個死 pid──一般輸入呢度──到達 `GetModuleFileNameEx` 同然後 `CloseHandle` 附帶一個空句柄。

   > [!NOTE]
   > **嚴重性更正**。呢個係最初寫成崩潰應用程式，基於推理嗰個關閉一個無效句柄喺嚴格句柄檢查下引起 `STATUS_INVALID_HANDLE`。嗰個係聲稱、唔係測量、同測量佢唔支援佢。一個獨立探測喺子進程下執行咗舊同已修正嘅防護喺 `ProcessStrictHandleCheckPolicy`：**全部兩個存活**、同仲然做咗一個控制關閉垃圾非空句柄──證明策略永遠冇被武裝同探測冇能區分。缺陷係真實（錯誤哨兵、兩個 API 呼叫喺空句柄、無效句柄關閉）但**無可觀察到嘅致命**：舊同新都產生一個空名稱對死 pid，所以 `has_restore_data()` 行為相同。佢係一個正確性同衛生修正、唔係崩潰修正。依賴無武裝探測嘅回歸測試係**移除**而唔係保留真空通過。
2. **自己 pid 重用讀作一個活躍擁有者** (`src/libslic3r/Format/bbs_3mf.cpp`)。Windows 重用已釋放 pid，所以崩潰之後直接重新啟動可以交給新例項已崩潰嘅 pid；檢查然後將進程同本身比較同拒絕復原。呢個係 2026-07-27 觀察嘅最可能解釋。鎖檢查而家拒絕一個符合目前進程個人 pid 嘅、同永遠唔讓一個空名稱同任何野比較相等。
3. **異常逃逸到啟動** (`src/libslic3r/Format/bbs_3mf.cpp`)。`load_string_file()` 坐喺 `try` 外面，所以一個無法讀取嘅鎖檔案──包括競爭條件度佢係刪除 `exists()` 檢查同讀之間──拋出超出 `has_restore_data()` 到 `EVT_RESTORE_PROJECT` 處理程式，度一個未處理拋出喺啟動時摧毀應用程式。佢而家拒絕、設定 `origin = "<lock>"` 所以呼叫者唔刪除一個備份，其擁有權係未知、同記錄原因。

回歸涵蓋：`tests/libslic3r/test_crash_restore.cpp`（目標 `crash_restore_tests`、喺維持 CTest 門內）──**29 個斷言喺 8 個測試案例、全部通過**。佢直接斷言 `OpenProcess` 哨兵不變同涵蓋死擁有者、自己 pid 重用、損壞、空同無法讀取鎖體。

嗰啲實際區分、誠實陳述嗰啲：

| 測試 | 冇佢嘅修正會失敗嗎？ |
| --- | --- |
| `has_restore_data treats its own pid in the lock as reuse` | **係**──喺修正前名稱比較符合同佢返回 false |
| `has_restore_data declines without throwing when the lock cannot be read` | **係**──喺修正前 `load_string_file` 拋出直接超出函數 |
| `OpenProcess reports failure with NULL, not INVALID_HANDLE_VALUE` | 否──佢記錄 Win32 合約防護依賴、同只會失敗如果 Windows 改變 |
| `get_process_name … nothing for a dead pid` | 否──舊同新都返回空；佢固定合約 `has_restore_data()` 依賴 |

> [!WARNING]
> 手動編輯 `BambuStudio.conf` 來分階段嗰個測試係一個陷阱。檔案結束附帶一個 `# MD5 checksum` 列、同當一個過時校和只記錄一個警告、**格式不良嘅 JSON 使應用程式無聲地回滾到 `BambuStudio.conf.bak`**──所以編輯睇起來被忽略。用一個真實 JSON 序列化器寫體同重新計算校和超越上至同包括最後 `}`。

- 單元測試：`tests/project_history/project_history_tests.cpp`
  （提交/列表/復原/遷移）。
- 透過 `deps/libgit2/libgit2.cmake` 加
  `find_package(libgit2 1.9 CONFIG REQUIRED)` 構建同連結；喺目前完整構建中編譯乾淨。
- 對話同選單/頂部欄項目點在 `docs/screenshots/version-history/` 底下截圖矩陣中擷取。
