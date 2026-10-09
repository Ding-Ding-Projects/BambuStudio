---
translation-of: self-update-diagnostic.md
source-sha256: 38dfdfe88d7e9685352f5162335a69206dcda48056a46de0d63f36ed199a1e83
review-status: agent-drafted
---

> 英文原文：[Self-update diagnostic](self-update-diagnostic.md)

# 自我更新診斷

已安裝嘅副本應該識自己更新：啟動嘅時候佢會叫 Squirrel 嘅 `Update.exe` 將最新發佈準備喺而家行緊嗰個版本隔籬，搞掂咗就出一個準備好橫幅（[應用程式更新](../windows/app-updates.md#automatic-update-of-an-installed-copy)）。之前從來冇一次執行證明過呢樣嘢真係會發生（[issue #46](https://github.com/Ding-Ding-Projects/BambuStudio/issues/46)）。`.github/workflows/diagnose-self-update.yml` 喺用完即棄嘅託管 Windows 執行器上裝一個舊發佈，好似捷徑咁啟動佢，再記低佢有冇自己準備好最新發佈。

## 點樣行

淨係可以手動觸發：喺 Actions 揀 **Diagnose self-update on hosted Windows**，撳 Run workflow，或者：

```
gh workflow run diagnose-self-update.yml -f from_tag=md3-v231 -f observe_seconds=900
```

| 輸入 | 預設 | 意思 |
| --- | --- | --- |
| `from_tag` | `md3-v231` | 要安裝、再由佢開始更新嘅已發佈版本；應該要比最新發佈舊 |
| `observe_seconds` | `900` | 睇住行緊嘅副本等佢更新幾耐，120 至 1500 秒 |

得一個工作，喺 `windows-2025` 上行，最多 45 分鐘，對資料庫淨係有讀取權限，亦淨係用今次執行自己嘅 token 嚟讀發佈。PowerShell 喺 `scripts/ci/Diagnose-SelfUpdate.ps1`。

## 佢做啲乜

1. 喺任何下載返嚟嘅嘢行之前，先讀 `from_tag`、最新發佈，同埋最新發佈嘅 `RELEASES` 檔案。
2. 經 `scripts/ci/Verify-HostedSquirrelInstall.ps1` 靜默安裝 `from_tag`。呢個腳本會驗證發佈、已安裝嘅執行檔同捷徑，亦會清走 token；之後再確認安裝根目錄入面啱啱好得嗰個發佈嘅 `app-<version>`。
3. 唔帶任何參數啟動安裝根目錄嘅 `bambu-studio.exe`，同兩個捷徑一樣。乜嘢偏好設定都唔改：**自動更新**（`auto_update`）預設係開，而新嘅執行器冇設定檔。收據會記低應用程式自己儲存嘅值。
4. 喺全新嘅設定檔上，首次執行嘅 **Setup Wizard** 會以模態對話框形式彈出，而啟動時嘅更新檢查要等佢關咗先會行。真人會行完佢嚟關佢；診斷就用 `WM_CLOSE` 關，每次都記低。
5. 每五秒用安裝程式首次啟動診斷自己嘅輔助函式（由 `scripts/ci/Diagnose-InstallerFirstRun.ps1` 載入，唔係抄過嚟）記低 `bambu-studio.exe`、`Update.exe` 同 `Setup.exe` 程序同應用程式嘅視窗，再讀安裝根目錄嘅 `app-<version>` 資料夾同通知紀錄。過咗 `observe_seconds` 就停；又或者一有較新嘅資料夾有齊佢嘅 `bambu-studio.exe`、`Update.exe` 做完、橫幅有紀錄（或者已經過咗一分鐘），又或者連續三次輪詢都冇應用程式同 `Update.exe` 喺度行，就提早停。
6. 經應用程式嘅視窗叫佢關閉，等佢將記錄寫出嚟，然後停晒仲行緊嘅嘢。
7. 如果有較新嘅 `app-<version>` 資料夾有齊佢嘅 `bambu-studio.exe`，就同 `Update.exe` 下載到安裝根目錄 `packages` 資料夾嘅完整套件比較：套件一定要係更新來源 `RELEASES` 嗰行講明嗰個（SHA-1 一樣），套件入面 `lib/<framework>/` 下面每個檔案都要喺資料夾度、大細一樣，而 `bambu-studio.exe` 嘅 SHA-256 亦要同套件嗰個一樣。
8. 跟住再啟動多一次安裝根目錄嘅 `bambu-studio.exe`，睇 90 秒，記低行緊嘅係邊個 `app-<version>`。

## 證據

全部都係文字：

- 應用程式嘅更新記錄行（`check new version` 同 `auto update:`）。發佈版會用一條編譯咗入去、俾未揀地區之前寫嘅記錄用嘅鑰匙加密記錄；診斷由 checkout 入面嘅 `src/libslic3r/LogSink.cpp` 讀返呢條鑰匙，再保留一份解碼咗嘅副本；
- 每個 `Update.exe` 程序，連同佢嘅命令列、父程序同退出代碼；
- Squirrel 嘅記錄、安裝根目錄嘅 `packages` 資料夾同本地 `RELEASES`，同埋更新來源提供嘅 `RELEASES` 檔案；
- 各個 `app-<version>` 資料夾、每個幾時出現，同埋較新嗰個同佢嘅完整套件比較嘅結果；
- 通知紀錄入面記低嘅準備好橫幅同失敗通知。橫幅係畫喺 3D 視圖入面，列舉視窗睇唔到，而且冇任何嘢會截圖；
- 應用程式同 `Update.exe` 嘅 Application Error、Windows Error Reporting 同 .NET Runtime 項目。

## 分類

`receipt.json` 會按以下次序決定，寫低其中一個值同佢嘅根據。

| 值 | 意思 |
| --- | --- |
| `app_crashed` | 觀察更新期間或者下次啟動時，有應用程式程序崩潰，用安裝程式首次啟動分類器嘅規則判斷：崩潰退出代碼，或者 `bambu-studio.exe` 有 Application Error 或 Windows Error Reporting 崩潰項目 |
| `updated_staged` | 較新嘅 `app-<version>` 資料夾連埋佢嘅 `bambu-studio.exe` 已經準備好，而且證明到係 `Update.exe` 做完嘅：有一次 `Update.exe --update` 以 0 退出（或者應用程式記錄咗退出代碼 0，又冇記錄話搵唔到較新嘅嘢），`update_failed` 列出嘅情況一樣都冇發生，Squirrel 本地嘅 `packages\RELEASES` 有列出呢個版本，完整套件嘅每個檔案都喺資料夾度、大細一樣，`bambu-studio.exe` 嘅 SHA-256 亦一樣，而且下次啟動行嘅係呢個資料夾。根據會逐樣講，再講係咪更新來源嘅版本同橫幅紀錄 |
| `update_failed` | `Update.exe` 出錯退出或者崩潰，又或者更新來源明明有較新版本、乜都冇準備好佢都以 0 退出，又或者應用程式記錄咗更新或者發佈檢查失敗，或者記低咗失敗通知；又或者較新嘅資料夾有咗 `bambu-studio.exe`，但證明唔到已經做完，根據會叫佢做部分準備（partial staging）同解釋原因 |
| `update_offered_not_staged` | 有更新提供咗（`Update.exe --update` 行過，或者應用程式記錄咗有較新發佈），但到最後都冇準備好，亦冇失敗，例如 `Update.exe` 仲下載緊 |
| `no_update_seen` | 以上都唔係；根據會講更新來源有冇較新版本，同埋 Setup Wizard 係咪仲開住 |

單單得一個較新嘅資料夾乜都證明唔到。`Update.exe` 會將套件直接解壓去 `app-<version>`，解壓完先至喺 `packages\RELEASES` 記低；做到一半失敗嘅話，寫咗出嚟嘅嘢會留低，要等佢下一次行先會刪走個資料夾。Squirrel 嘅啟動器會開版本最高嘅 `app-<version>`，唔理佢齊唔齊。應用程式本身都一樣咁嚴：要 `Update.exe` 以 0 退出、而且真係有較新嘅資料夾，先算更新咗。`scripts/ci/Test-SelfUpdateClassification.ps1` 會喺安裝任何嘢之前，用假設嘅事實（例如資料夾準備好咗但 `Update.exe` 失敗）行一次分類。

分類就係結果：只有讀唔到發佈、驗證或者安裝唔到發佈，或者啟動唔到安裝根目錄嘅 `bambu-studio.exe`，工作先會失敗。

## 點樣睇證據

工作會上載 `self-update-<run id>-<attempt>`，保留七日，入面淨係有 `.json`、`.jsonl`、`.txt` 同 `.log` 檔案：`preflight.json`、`install-receipt.json`、`feed-RELEASES.txt`、`receipt.json`（入面有 `staged_candidate`，即係搵到嘅較新資料夾，同 `staged_files`，即係佢同套件比較嘅結果），同埋每次啟動一個資料夾（`observe`，搵到較新資料夾嘅話仲有 `next-start`），入面有 `samples.jsonl`、`process-events.json`、`update-log-lines.txt`、`notification_history.json` 同 `logs/`（Squirrel、啟動器追蹤、解碼咗嘅應用程式記錄同事件記錄項目）；`observe` 仲有 `squirrel-update-lines.txt` 同 `local-RELEASES.txt`。

## 限制

- 託管執行器冇 GPU，亦冇人坐喺佢嘅桌面前面。關 Setup Wizard 唔等於行完佢，所以冇設定任何打印機。真機上嘅更新先係最後嘅確認。
- 診斷唔會撳 **重新啟動以安裝更新** 條連結。經安裝根目錄嘅下次啟動，就係一個人下次打開應用程式時會得到嘅嘢。
