---
translation-of: export-everything.md
source-sha256: 1af8a6372f52d411ff1ada3686cafadb0b0deda5377a3e53bdc8111a25ecf789
review-status: agent-drafted
---

> 英文原文：[Export everything, in every format](export-everything.md)

# 匯出每樣嘢，用每種格式

**表面**：應用程式擁有嘅每一筆紀錄都通過一個共用嘅 `ExportDialog`（`src/slic3r/GUI/Export/ExportDialog.{hpp,cpp}`）。佢背後嘅引擎係 wx 自由嘅（`src/slic3r/GUI/Export/ExportFormats.hpp`、`ExportEverything.{hpp,cpp}`）同每個表面嘅數據集構建器住喺 `src/slic3r/GUI/Export/ExportDatasets.{hpp,cpp}`。

| 表面 | `Export…` 控制項喺邊度 | 數據集種類 |
| --- | --- | --- |
| 項目版本歷史 | `File ▸ Version history…` → **Export…** 按鈕 | 表格式（每個快照提交一行） |
| 偏好（`BambuStudio.conf`） | 偏好底行 → **Export preferences…**；也 `File ▸ Export ▸ Export preferences…` | 結構式（節 → 鍵 → 值） |
| 預設（打印 / 墨水 / 打印機） | 預設工具欄 `download` 圖示喺每個設定標籤上 Save / Delete 旁邊 | 結構式（元數據 + 每個選項如 `.ini` 序列化佢） |
| 對象列表 | 對象右鍵單擊選單 → **Export object list…**；也 `File ▸ Export ▸ Export object list…` | 表格式（索引、名稱、可打印、實例、件、修飾符、負/支撐體積、刻面、大小 X/Y/Z、體積名稱） |
| 打印統計 | `File ▸ Export ▸ Export print statistics…`（一旦打印盤被切片啟用） | 結構式（每模式時間按移動類型 / 角色 / 層、每擠出機體積、沖洗、墨水變化、墨水直徑/密度/成本） |
| 整個資料夾 | `File ▸ Config profiles & backup…` → **Export everything…**（不變，睇 [config-profiles-backup.md](config-profiles-backup.md)） | 目錄嘅 ZIP |

通知歷史同變更日誌查看器仲未喺呢棵樹度；當佢哋嘅路徑登陸時佢哋得到 `Export…` 項目通過相同對話框通過構建 `Dataset` 同呼叫 `ExportDialog::run`。

## 行為

- **每個數據集嘅每個格式。** JSON、JSON Lines、YAML、TOML、XML、CSV、TSV、Markdown 同 HTML 每次都被提供。自然嘅數據格式首先出現同標記為*建議*（表格 → CSV/TSV、結構 → JSON/YAML/TOML、散文 → Markdown/HTML）。
- **無損 / 有損徽章同精確原因。** `compute_loss_report` 檢查真實數據，唔係抽象格式：TOML 匯出只係有損當數據實際包含空值（TOML 冇任何）、XML 匯出只係當字串攜帶 XML 1.0 禁止嘅 C0 控制字符、JSON 匯出只係當數字係 NaN/Infinity。格式卡顯示*失去：*同*註解：*列表喺任何野被寫之前，狀態線重複匯出時刻嘅丟失文字。冇野曾經被靜靜刪掉。
- **CSV/TSV 從未失去巢狀。** 結構數據集寫成 CSV/TSV 被展平到 `path,type,value` 行带 RFC 6901 JSON 指標路徑（`/inner/empty list`、`/a~1b`），空數組/對象同空值保留一行，所以樹可重構。表格 CSV 遵循 RFC 4180 引號；TSV 轉義 `\t \n \r \\` 帶反斜線。
- **每個檔案說明佢嘅模式、編碼同行終止。** JSON 得到一個信封（`schema`、`schemaVersion`、`dataset`、`kind`、`encoding: UTF-8`、`lineEnding`、`generator`、`columns`、`data`）；JSONL 一份首行紀錄帶 `"_type":"header"`；YAML/TOML/Markdown 一條評論行；XML 屬性喺 `<export>` 根上；HTML `<meta>` 標籤。CSV 同 TSV 冇評論句法，所以佢哋嘅頭部旅行喺 `<file>.meta.json` 伴隨檔案也記錄分隔符、引號規則同每列聲明類型。
- **永遠 UTF-8；LF 或 CRLF 按選擇；BOM 選擇進入。** 行終止選擇被記錄喺頭部。字節序標記只係為電子表格存在，佢錯誤讀取 CSV 冇佢同預設係關。
- **檔案係 ZIP 或 7z，從未係其他嘢。** ZIP 喺進程內被寫入通過供應商 miniz（Deflate）同從未加密。對話框說佢而唔係提供密碼。7z 執行喺 PC 上發現嘅 7-Zip 命令列同公開佢整個選項表面：方法（LZMA2 / LZMA / PPMd / BZip2 / Deflate）、級別（存儲…超）、字典大小、字大小、實心 on/off 同實心塊大小、線程計數、分割體積、AES-256 密碼同**加密頭**。每個選項攜帶一個成本提示（RAM 壓縮同提取、單一 vs 多線程、實心成本喺提取）。帶密碼設定同頭留喺清除對話框顯示紅色警告檔案名保持可讀；佢從未呼叫呢樣檔案受保護。
- **成員路徑係相對同安全。** `sanitize_archive_path` 去掉前導斜線同 `./`、轉換反斜線、拒絕驅動字母、UNC 前綴、空段、控制字符同任何 `..` 段。7z 成員喺新臨時目錄度被暫存同用 `*` 從果個目錄度新增，所以 7-Zip 自己從未睇到絕對路徑。
- **搜尋同正則構建器。** 對話框攜帶共用嘅 `SearchField` 對格式行同選項行帶 `.*` 切換同完整錨定正則構建器。
- **本地瀏覽控制項**旁邊輸出路徑（`wxFileDialog`、過濾到選擇嘅副檔名）；副檔名遵循選擇嘅格式或檔案。**定位 7z.exe…**係第二個本地瀏覽固定 7-Zip 可執行檔喺 `BambuStudio.conf`（`[export_everything] seven_zip_path`）；最後匯出目錄記住喺度（`last_dir`）。
- **非阻止結果。** 成功發佈標準「匯出完成」通知帶打開資料夾操作；失敗係對話框度一條內聯狀態線錯誤顏色帶真實原因（`7-Zip exited with code 2 (fatal error)`、`Refused unsafe archive member path: ../x`）。

## 配置

| 鍵（`BambuStudio.conf`、節 `export_everything`） | 意思 |
| --- | --- |
| `last_dir` | 最後成功匯出嘅目錄；下次預設位置。 |
| `seven_zip_path` | 完整路徑到 `7z.exe` 通過*定位 7z.exe…*選擇；喺自動搜尋前檢查。 |

7-Zip 發現順序：固定路徑、接著 `7z.exe` / `7za.exe` 喺 `PATH` 上、接著 `%ProgramFiles%\7-Zip`、`%ProgramW6432%\7-Zip`、`%ProgramFiles(x86)%\7-Zip`、接著 `%LOCALAPPDATA%\Programs\7-Zip`。

## 失敗模式

- **7-Zip 冇發現。** 7z 選項保持可見帶誠實狀態（`7-Zip not found. Install 7-Zip or locate 7z.exe. Searched: …`），匯出按鈕被禁用帶果個原因作為佢嘅工具提示，ZIP 同平檔案保持工作。冇靜靜從 7z 退回到 ZIP。
- **密碼不符** 禁用匯出帶「兩個密碼欄位不同。」
- **有損格式選擇。** 允許，重點係用戶被告知咗。徽章、*失去：*列表同匯出時間狀態線都話咗去咗咩。
- **7-Zip 退出碼 1**（警告）仍然計為寫入；警告文字被追加到成功摘要。碼 2/7/8/255 失敗帶 7-Zip 嘅意思拼寫出來。
- **不安全成員路徑**（只可到達通過編程錯誤，由於對話框自己命名成員）喺任何野被寫前中止。

## 安全考量

- 7z 密碼被傳遞到 `7z.exe` 作為 `-p` 參數對果個進程，從未堅持，同編輯到 `-p***` 喺對話框顯示嘅命令列後面。喺 Windows 一條命令列係同一用戶嘅其他進程可見貫穿 7-Zip 進程嘅壽命；ZIP 路徑涉及冇外部進程。
- 預設同偏好匯出包含活紀錄包含嘅一切。`BambuStudio.conf` 可以持有訪問碼同令牌完全如完整數據備份；匯出係用戶嘅故意操作從一個對話框名稱數據集。
- 檔案提取安全被強制喺寫側（`sanitize_archive_path`）所以冇檔案呢個功能產生可以攜帶路徑逃脫成員。

## 驗證

- `tests/export_everything/export_everything_tests.cpp`（Catch2、目標 `export_everything_tests`、只連結 `test_common` 同 `miniz`）：逃脫輪換（JSON、CSV 引號、TSV 轉義、XML 實體、TOML/YAML 引號、Markdown 單元、CRLF）、JSON 信封/JSONL 頭、YAML 巢狀同永遠引號標量、TOML 表 / 表數組 / 空值省略帶佢嘅丟失報告、XML 結構、CSV/TSV 帶伴隨檔案、嵌套數據 CSV 展平帶 JSON 指標轉義、Markdown/HTML 渲染同佢哋嘅丟失報告、NaN/控制字符邊界情況、檔案路徑消毒（`..`、驅動字母、UNC、空段）、一個 miniz ZIP 輪換讀回 miniz 讀取器、完整 7-Zip 開關映射每個方法/級別/字典/字/實心/線程/體積/密碼/頭組合包括密碼編輯、同 `run_export` 寫一個數據檔案 + 伴隨、一個 ZIP 同誠實「7-Zip 冇發現」錯誤。
- 最後手執行結果：`All tests passed (247 assertions in 13 test cases)`。
- 句法檢查：每個改變 `.cpp` 喺 `cl /Zs` 編譯帶 GUI 包括設定（Tab.cpp、GUI_Factories.cpp 同 MainFrame.cpp 需要項目嘅預編譯頭力迫包括，因為真實構建做咗）。

## 建議文章

- [Config profiles & full-data backup](config-profiles-backup.md)：完整數據資料夾 ZIP 呢個功能坐旁邊。
- [Project version history](project-version-history.md)：首個表面連接到對話框。
- [Preferences auto-history](preferences-history.md)：設定紀錄偏好匯出快照。
- [Non-blocking notifications](non-blocking-notifications.md)：匯出完成 toast 點行為。
