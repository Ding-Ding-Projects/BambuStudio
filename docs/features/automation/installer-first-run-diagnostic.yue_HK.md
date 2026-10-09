---
translation-of: installer-first-run-diagnostic.md
source-sha256: f3a9b8989383b77942e35eebe22662dbab5044a47aaeb1c026b5f847f563c512
review-status: agent-drafted
---

> 英文原文：[Installer first-run diagnostic](installer-first-run-diagnostic.md)

# 安裝程式首次啟動診斷

互動安裝嘅時候，Squirrel.Windows 會趁安裝程式仲未收尾，自己用 `--squirrel-firstrun` 啟動應用程式。其他託管安裝檢查全部都用 `--silent` 安裝，而靜默安裝乜都唔會啟動，所以喺託管執行器上從來冇人睇過呢次啟動。`.github/workflows/diagnose-installer-first-run.yml` 喺用完即棄嘅託管 Windows 執行器上觀察呢次啟動，同埋捷徑用嘅、經安裝根目錄嘅啟動。第一次啟動喺啟動器嗰邊點樣做，見[應用程式更新](../windows/app-updates.md#shortcuts-and-install-events)。

## 點樣行

淨係可以手動觸發：喺 Actions 揀 **Diagnose installer first run on hosted Windows**，撳 Run workflow，或者：

```
gh workflow run diagnose-installer-first-run.yml -f tag=md3-v225 -f observe_seconds=180
```

| 輸入 | 預設 | 意思 |
| --- | --- | --- |
| `tag` | `md3-v225` | 要安裝嘅已發佈版本；佢嘅目標一定要係一個 commit |
| `observe_seconds` | `180` | 每次啟動喺觸發完成之後再睇幾耐，30 至 420 秒 |

兩個工作喺 `windows-2025` 上並行，每個最多 30 分鐘，對資料庫淨係有讀取權限，亦淨係用今次執行自己嘅 token 嚟讀發佈。

| 工作 | 安裝 | 階段 |
| --- | --- | --- |
| `interactive` | `Setup.exe` 唔帶參數、喺可見視窗入面行，最多等 600 秒 | `install-firstrun`：Squirrel 自己做嘅啟動。之後停晒安裝根目錄底下嘅程序，再做 `control-stub`：好似捷徑咁啟動安裝根目錄嘅 `bambu-studio.exe` |
| `silent` | `Setup.exe --silent` | `install-silent`：理應乜都唔啟動，順便驗證分類本身。之後 `first-stub`：經安裝根目錄嘅 `bambu-studio.exe` 做第一次啟動 |

兩個工作都經 `scripts/ci/Verify-HostedSquirrelInstall.ps1` 安裝（互動嗰個加 `-Interactive`），所以發佈會好似其他託管檢查咁逐樣核對：已發佈嘅摘要、`RELEASES` 行、套件合約、已安裝嘅執行檔同捷徑。任何下載返嚟嘅嘢行之前，工作流程嘅憑證都會清走。PowerShell 放喺 `scripts/ci/Diagnose-InstallerFirstRun.ps1`。

## 每個階段記錄啲乜

由安裝（或者 stub）開始，一直到佢完成之後再過 `observe_seconds`，每五秒記一次：

- 每個 `bambu-studio.exe`、`Update.exe` 同 `Setup.exe` 程序：程序 ID、父程序、路徑、命令列、主視窗 handle 同標題，同埋有冇回應；
- 應用程式程序嘅每個頂層視窗，連 z-order、可唔可見、有冇最小化同大小，仲有前景視窗同擁有佢嘅程序。

程序開始同結束事件會補上準確嘅開始時間同退出碼，建立事件就補上佢哋嘅命令列。不過到目前為止，託管執行器上嘅結束事件淨係見過 `Update.exe` 同 `Setup.exe` 嘅，所以 `bambu-studio.exe` 嘅退出碼，要有一次輪詢捉到個程序、握住佢嘅 handle 先會知道。每個階段之後會將以下嘢以純文字儲存：Squirrel 嘅記錄（`%LOCALAPPDATA%\SquirrelTemp` 同安裝根目錄入面每個 `*.log`）、`%TEMP%\bbs-launcher-trace.log`、`%APPDATA%\BambuStudio\log` 入面最新嘅檔案，同埋 Application 事件記錄入面提到應用程式嘅錯誤同當機報告。分類會將嗰段 Application 記錄文字讀返：每個 Application Error 項目都講出出錯程序嘅 ID、例外代碼、出錯位移同出錯模組；每份 Windows Error Reporting 當機報告都講埋呢啲，淨係冇程序 ID。`preflight.json` 記錄之前冇任何安裝、顯示卡，同埋工作同主控台嘅工作階段。

## 分類

每個階段寫一份 `receipt.json`，入面係以下其中一個值，按呢個次序決定。安裝事件程序（`--squirrel-install` 同其他事件）同 Squirrel 嘅 stub 都唔當係應用程式。階段開始之前已經喺度行緊嘅程序，同埋出錯程序喺出錯嗰陣用自己條命令列開出嚟嘅子程序（Windows Error Reporting 為出錯程序整嘅副本就係咁樣出現），都唔計；收據入面嘅 `not_counted` 會列出呢兩種，連埋原因。

| 值 | 意思 |
| --- | --- |
| `started_crashed` | 有應用程式程序以當機退出碼結束（NTSTATUS 錯誤，即 `0xC0000000` 或以上，但唔包括啟動器自己嘅 `0xFFFFFFFF`），或者呢個階段嘅 Application 記錄有 `bambu-studio.exe` 嘅 Application Error 或 Windows Error Reporting 當機項目。佢壓過其他所有值，唔理之前出過咩視窗 |
| `started_visible` | 冇嘢當機，而且有應用程式程序出咗可見視窗，到最後仍然喺度行，又或者嗰個視窗係主視窗（標題係 `<project> - Bambu Studio`）；收據會講佢有冇做過前景視窗 |
| `started_hidden` | 到最後仍然有應用程式程序喺度行，但冇一個可見、冇最小化嘅視窗 |
| `started_exited` | 有應用程式程序行過，冇當機就結束咗；收據會列出佢嘅退出碼（用 Windows 顯示嘅寫法，`0xFFFFFFFF` 即係啟動器自己嘅 `-1`）同佢活咗幾耐，根據會講佢結束之前有冇出過可見視窗，例如冇標題嘅啟動畫面 |
| `not_started` | 一個應用程式程序都見唔到 |

`started_crashed` 嘅時候，收據入面嘅 `crash` 會講出出錯程序嘅 ID 同角色、例外代碼，Application 記錄有講嘅話仲有出錯位移同出錯模組，加上退出碼、活咗幾耐、當機之前有冇出過可見視窗（`window_shown_before_crash`，連埋視窗標題），同埋佢喺出錯嗰陣整出嚟嘅副本。md3-v230 就係呢種情況：佢第一次啟動嗰陣出過個 480 乘 480、冇標題嘅啟動畫面，而每次啟動最後都喺 `BambuStudio.dll` 入面存取違規（`0xC0000005`）結束。

`application_faults` 列出呢個階段每個當機項目，知道嘅話仲會寫埋項目講嗰個程序嘅退出碼。每一個項目都會令呢個階段變成 `started_crashed`，唔理個程序之後用咩退出碼結束：嗰個退出碼要啱啱有輪詢捉到個程序先會知道，所以佢只係用嚟描述當機，唔會決定分類。md3-v229 就係咁嘅例子：`BambuStudio.dll` 初始化嗰陣出錯，Windows 記低咗存取違規，之後啟動器用 `-1` 退出（佢嘅追蹤記錄寫住 `EXIT -1: BambuStudio.dll load failed, error=1114`）。嗰幾次啟動都係 `started_crashed`。有輪詢捉到啟動器嘅話，`crash.exit_code_hex` 係 `0xFFFFFFFF`，根據會講啟動器之後用佢退出；冇輪詢捉到嘅話，根據會講冇記錄到退出碼。

收據之前有嘅欄位全部保留原名。`crash`、`application_faults` 同 `not_counted` 係新加嘅，`exit` 亦多咗 `visible_window`。

分類本身就係結果：只有喺發佈核對唔到或者裝唔到，又或者安裝根目錄嘅 `bambu-studio.exe` 啟動唔到嘅時候，工作先會失敗。步驟摘要會逐個階段列出結果同根據。

## 點樣睇證據

每個工作會上載 `installer-first-run-<job>-<run id>-<attempt>`，保留七日。入面有 `preflight.json`、安裝檢查嘅 `install-receipt.json`，同埋每個階段一個資料夾，裝住 `receipt.json`、`samples.jsonl`（每行一次輪詢）、`process-events.json` 同 `logs/`。入面冇任何圖片。

加入咗首次啟動交接嘅啟動器，每行追蹤記錄開頭都有本地時間同程序 ID，所以可以分得清每個程序寫咗邊幾行：`launcher start:` 連命令列、`squirrel event --squirrel-firstrun:` 連父程序、等待、stub 同結果、提早退出嘅 `EXIT -1:`，同埋應用程式返回時嘅 `bambustu_main returned`。一個程序結束時冇最後嗰行，即係佢喺應用程式入面結束。之前建置嘅版本（包括 `md3-v225`）寫嘅係舊格式，冇時間亦冇程序 ID，而且第一次啟動係自己做。

## 限制

- 託管執行器冇 GPU，所以應用程式用隨附嘅 Mesa 軟件渲染器，而且冇人坐喺佢個桌面前面。喺嗰度見到可見視窗，係啟動行得通嘅有力證據；但唔能夠證明真人嘅桌面會將視窗擺喺最前。真機安裝始終係最後一關。
- 記錄來自一個冇帳戶嘅全新執行器設定檔，不過係原封不動咁上載；喺其他地方引用之前請先睇一次。
- 當機欄位係由託管執行器寫嘅英文 Application 記錄文字讀出嚟，主視窗就靠佢用隨附顯示名稱嘅標題認出嚟。用其他語言嘅執行器，或者改咗名嘅應用程式，仍然會由退出碼得出 `started_crashed`，不過冇出錯位移同模組；而佢要算可見啟動，就要有程序到最後仍然喺度行。
- 合約：`node --test ui-md3/tests/squirrel-install-events.test.mjs` 固定咗只可手動觸發、唯讀權限、釘死版本嘅 actions、時間上限、七日保留期、經環境變數傳入嘅輸入、安裝檢查預設靜默、唔會截圖、每個分類值，同埋令當機優先嘅次序。實際運行行為只有觸發呢個工作流程先確立到。
