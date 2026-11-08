---
translation-of: extensions.md
source-sha256: 7809a4af524d89773befef83c920c41daad6d5953554604cb3c6955d47bb3ffa
review-status: agent-drafted
---

> 英文原文：[Extensions](extensions.md)

# 擴充功能

**表面**：檔案 > 擴充功能、指令面板（`Ctrl+Shift+F`，打「Extensions」）、全部功能頁，或者幫助 > 設定 Bambu Studio 嘅擴充功能一步 → `ExtensionsPanel`（`src/slic3r/GUI/Extensions/ExtensionsPanel.{hpp,cpp}`），係本機工具分頁入面嘅一頁。決定全部喺冇 wxWidgets 嘅模組度做：`ExtensionCore`（清單檔、掛鈎、權限、同意、請求、答覆同執行紀錄）、`ExtensionProcess`（行一個程式）、`ExtensionHost`（裝咗嘅資料夾、內置擴充功能同儲存咗嘅狀態）同 `ExtensionContext`（請求入面有乜，同埋匯出 G-code 嘅註解行）。`ExtensionRuntime` 用一條工作執行緒將佢哋接駁落應用程式度。

設計：[design/local-tools-2026-11](../../../design/local-tools-2026-11/README.md)，Extensions 板。

擴充功能係一個資料夾，入面有 `manifest.json` 同一個用任何語言寫嘅程式。Bambu Studio 喺佢要求嘅時候行呢個程式，經標準輸入送一個 JSON 請求畀佢，再由標準輸出讀返一個 JSON 答覆。程式喺 Bambu Studio 外面行，一次行一個，喺工作執行緒度行，所以一個慢或者壞咗嘅擴充功能唔會令應用程式凍結或者當機。

## 開一個擴充功能

每個擴充功能，連內置嗰啲，一開始都係關咗嘅。開佢嘅時候會彈出提示，列明佢要求嘅每一樣嘢。**完全照呢個清單允許**會為呢個版本同呢個清單開佢；**保持關閉**會記低你拒絕咗；直接關咗個提示就乜都唔記，下次會再問。

新版本或者改咗嘅清單會再次將佢關咗，直到你再答提示為止；提示會將每個權限標做**新**或者**之前已允許**。右上角嘅主開關**擴充功能開**一下就關晒所有擴充功能，但唔會忘記你允許過乜。

## 呢一頁

- **由資料夾加入**會將你揀嘅資料夾（入面要有 `manifest.json`）複製入擴充功能資料夾。如果個 id 已經裝咗，會先問：**取代**定係保留已裝嘅版本，兩個版本同佢哋嘅權限數量都會顯示。清單檔壞咗就會講邊度錯，乜都唔會加。
- **打開資料夾**會打開 `%APPDATA%\BambuStudio\extensions`。**重新載入**會再讀一次每個資料夾，例如你改完清單檔之後。
- **作者指南**會打開呢篇文章。
- **搜尋**對名稱、id、描述、掛鈎、指令同權限，用共用嘅搜尋欄（普通文字，或者 `.*` 用正規表示式）。
- **篩選**：全部、開、關、等緊你、清單檔錯誤同內置，每個都有數量。
- **每次執行嘅限制**：時間上限（1 至 60 秒，一開始係 10 秒）、最大答覆（1 至 1024 KB，一開始係 64 KB），同埋連續失敗幾多次就自動關（0 至 20，一開始係 3；0 即係永遠唔會）。
- **過去 7 日**：幾多次答咗、超時、太大、唔係有效 JSON、犯咗規則、失敗，或者被停止。
- 有嘢行緊嘅時候，工具列會講係邊個擴充功能同邊個掛鈎、有幾多次喺度等，仲有**停止**：十分一秒之內結束嗰次執行，並清走排隊嘅。被停止嘅執行永遠唔會當做失敗。

每個擴充功能有：

| 分頁 | 顯示乜 |
| --- | --- |
| 概覽 | 佢嘅掛鈎（呢個版本未會行嘅掛鈎會講明）、佢嘅指令同**執行**、佢嘅程式同 API 版本，同埋所有掛鈎嘅表 |
| 權限 | 每個權限：已允許、而家問緊、唔允許：保持關閉，或者冇要求，同埋永遠唔會送出嘅嘢 |
| 執行紀錄 | 佢嘅執行，最新嘅喺頭：幾時、掛鈎或者指令、用咗幾耐、答覆大細同結果；**匯出執行紀錄**會將所有擴充功能嘅每次執行儲存做 JSON |
| 清單檔 | 磁碟上嘅 `manifest.json` 原文，同埋係咪有效，或者錯喺邊 |

**用樣本試吓**會用一個由虛構數值組成嘅請求行佢一次，每部分都標住 `"sample": true`。佢嘅備註會顯示；佢答嘅註解行、檔案、清單行同阻止只會報告，永遠唔會套用。旁邊有**打開佢嘅資料夾**同**移除**（問過你之後刪除個資料夾）。

## 內置擴充功能

佢哋係 `resources/extensions` 入面嘅 Windows PowerShell 指令碼，第一次用嘅時候裝入擴充功能資料夾。Bambu Studio 只會喺內置擴充功能嘅檔案仲係佢當初裝嘅樣嗰陣更新佢（`.builtin.json` 標記記住佢哋嘅雜湊值）；你一改咗，更新就停，呢頁會講明。你移除咗嘅內置擴充功能會保持移除，直到你再加返佢嘅資料夾。

| 擴充功能 | 幾時行 | 做乜 |
| --- | --- | --- |
| Slice report | 切片之後；佢嘅指令 **Save a slice report** | 一條備註寫住打印時間、每個槽嘅墨水同層數，再加警告數量。個指令會答一份 Markdown 報告畀你儲存 |
| Plate checklist | 切片之後同發送之前 | 切片之後，有嘢值得再望多眼先會出備註：切片警告、8 個鐘或以上嘅打印、30 次或以上換墨水、要 500 g 或以上嘅槽。發送之前，同樣幾點會變做清單行 |
| G-code time stamp | 匯出 G-code 之後 | 喺檔案頭尾加註解行，寫住匯出時間，同打印板嘅時間、墨水同層數 |

佢哋用 `powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File <script>` 行。如果部電腦嘅政策禁止 PowerShell 指令碼，佢哋就會失敗，呢頁會顯示結束代碼。

## 掛鈎

| 掛鈎 | 幾時 | 收到 | 可以答 | 呢個版本 |
| --- | --- | --- | --- | --- |
| `plate.sliced` | 一塊打印板切完片之後；每個切片結果一次，所以匯出一塊已經切好嘅打印板唔會再行 | `slice`、`printer` | 備註 | 會行 |
| `gcode.exported` | 匯出純 G-code（`.gcode`）之後，等切片工作用完個檔案先 | `slice`（有檔案名，永遠冇佢嘅資料夾）、`printer` | 檔案頭同尾嘅註解行 | 會行 |
| `command` | 你喺呢頁或者指令面板開始佢其中一個指令嘅時候 | `model`（揀咗嘅嘢）、`slice`（打印板切咗片嘅話）、`printer` | 備註、一個畀你儲存嘅文字檔 | 會行 |
| `print.before_send` | 喺打印對話框為呢個項目切咗片嘅打印板撳**發送**嗰陣，喺應用程式自己嘅打印前檢查之前 | `slice`（一次發送所有打印板就係全部加埋）、`printer`、`send` | 清單行、連原因嘅阻止、備註 | 會行 |
| `model.imported` | 載入檔案、有物件加入之後（匯入、拖放、項目），等成個載入做完先 | `model`（淨係加入咗嘅物件，連檔案名） | 備註 | 會行 |
| `print.finished` | Bambu Studio 喺裝置頁揀咗嘅打印機上見過行緊嘅打印完成、失敗或者取消嗰陣（每 3 秒睇一次，淨係有擴充功能開咗佢先睇） | `print`、`printer` | 備註 | 會行 |

清單檔可以宣告呢個版本未會行嘅掛鈎；佢會照樣列出，**用樣本試吓**都仲可以試佢。

## 發送打印之前

喺打印對話框撳**發送**（或者將打印板加入打印隊列）嗰陣，會先逐個問每個為 `print.before_send` 開咗嘅擴充功能，排喺所有等緊嘅執行前面。一個都冇開嘅話就乜都冇變：冇視窗，冇延遲。否則會出一個視窗，講係邊個擴充功能檢查緊、檢查咗幾耐，有**返回**同**唔檢查直接發送**（即刻停咗嗰次執行；記錄會寫你停咗佢，永遠唔會當做失敗）。如果另一個擴充功能仲做緊之前開始嘅執行，視窗會講明，等佢做完。

全部擴充功能答完之後：

| 答覆 | 會點 |
| --- | --- |
| 淨係有通過嘅行 | **全部檢查通過**會顯示一陣，跟住繼續發送 |
| 有警告或者唔合格嘅行、備註，或者有執行冇有效答覆 | 全部按擴充功能分組顯示，有**打開擴充功能**、**返回**同**發送** |
| 有效答覆有 `"block": true` | 唔會發送。原因排先，跟住係每個答覆，淨係有**打開擴充功能**同**返回** |

只有有效答覆先可以阻止打印，而且只限你准咗 `send.block` 嘅擴充功能：冇呢個權限而阻止就係犯規則；超時、失敗或者答錯嘅執行會顯示做冇答覆，永遠唔會攔住打印（不過仍然會計入自動關閉嘅次數）。想發送一個被擴充功能阻止咗嘅打印，就改打印板或者墨水對應，或者關咗嗰個擴充功能。**打開擴充功能**會關咗打印對話框，再喺要睇嗰個擴充功能度打開呢頁。

擴充功能之後，應用程式自己嘅打印前檢查照舊跟住嚟。每次執行都會喺執行紀錄分頁度，以「發送打印之前」同打印板列出。

## 權限

| 權限 | 允許乜 |
| --- | --- |
| `slice.read` | `slice` 部分：時間、層數、每個槽嘅墨水、警告、打印板類型 |
| `model.read` | `model` 部分：物件名稱、實例數量、三角形數量、尺寸、揀咗嘅嘢 |
| `printer.read` | `printer` 部分：打印機預設、型號、噴嘴尺寸同墨水預設；發送打印之前，仲有 `send` 部分；打印完之後，仲有 `print` 部分 |
| `notes.show` | 以通知顯示備註 |
| `send.warn` | 發送打印之前嘅清單行 |
| `send.block` | 阻止一個打印發送出去，要講原因 |
| `gcode.comment` | 匯出 G-code 嘅註解行 |
| `file.save` | 提供一個文字檔，由你揀儲存位置 |

擴充功能唔可以收嘅部分唔會放入請求。名稱似秘密嘅值（密碼、token、存取碼、cookie、API key 之類）無論權限點都會喺每一層移除。答覆用咗未獲允許嘅嘢就會成個掉咗，並記錄做壞咗。

密碼、存取碼、帳戶 token 同打印機序號永遠唔會送出。不過擴充功能始終係以你身份行嘅程式，所以 Bambu Studio 阻止唔到佢讀你自己嘅檔案：只允許你信得過嘅擴充功能。

## 寫一個擴充功能

喺 `%APPDATA%\BambuStudio\extensions` 開一個資料夾（或者喺任何地方開，再用**由資料夾加入**）：

```
hello/
  manifest.json
  hello.ps1
```

### manifest.json

```json
{
  "id": "com.example.hello",
  "name": "Hello",
  "version": "1.0.0",
  "apiVersion": 1,
  "description": "Says how long the plate takes.",
  "author": "You",
  "run": { "windows": "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File hello.ps1" },
  "hooks": ["plate.sliced"],
  "actions": [{ "id": "greet", "title": "Say hello" }],
  "permissions": ["slice.read", "notes.show"]
}
```

| 欄位 | 規則 |
| --- | --- |
| `id` | 必填。細楷字母、數字、點同連字號，最少一個點，例如 `com.example.hello`；最多 100 個字元。兩個資料夾用同一個 id 就兩個都唔收 |
| `name` | 必填，最多 80 個字元 |
| `version` | 必填，`MAJOR.MINOR.PATCH` |
| `apiVersion` | 必填，`1`。比應用程式支援嘅更新嘅數字會被拒絕，呢頁會講係邊個 |
| `description` | 最多 500 個字元 |
| `author` | 最多 80 個字元 |
| `run.windows` | 必填。指令行，用 Windows 嘅引號同反斜線規則拆開；最多 1000 個字元 |
| `hooks` | 掛鈎 id；有 actions 嘅話會自動加 `command` |
| `actions` | 最多 10 個指令：`id`（細楷字母、數字同連字號，最多 40）同 `title`（最多 60） |
| `permissions` | 權限 id |

其他欄位、唔識嘅掛鈎或者唔識嘅權限都會令清單檔無效，呢頁會顯示係邊個。清單檔最多 64 KB。

### 程式

- 冇殼層。`run.windows` 嘅第一個字如果係資料夾入面一個檔案嘅名，就用嗰個檔案；唔係就搵 Windows 系統資料夾（同 Windows PowerShell 嘅資料夾）入面嘅程式，再唔係就搵 `PATH`。用 `..` 走出資料夾嘅路徑會被拒絕。
- 佢喺自己嘅資料夾度開始，用一個細細嘅環境：`SystemRoot`、`windir`、`SystemDrive`、`ComSpec`、`PATH`、`PATHEXT`、`TEMP`、`TMP`、`USERPROFILE`、`APPDATA`、`LOCALAPPDATA`、`ProgramData`、`ProgramFiles`、`ProgramFiles(x86)`、`NUMBER_OF_PROCESSORS`、`PROCESSOR_ARCHITECTURE`、`OS`、`PSModulePath`，同埋 `BAMBU_STUDIO_EXTENSION_API=1`。應用程式其他嘅嘢都唔會繼承，連 handle 都冇。
- 佢由標準輸入讀一個請求（UTF-8 JSON；讀完之後輸入會關閉），向標準輸出寫一個答覆（UTF-8 JSON），然後用代碼 0 結束。標準錯誤會保留頭 4 KB，只係用嚟診斷。
- 佢行超過時間上限，或者寫多過最大答覆，就會被結束，佢開過嘅所有嘢都會一齊結束（Windows job object）。

### 請求

```json
{
  "apiVersion": 1,
  "hook": "plate.sliced",
  "action": "",
  "extension": { "id": "com.example.hello", "version": "1.0.0" },
  "context": {
    "slice": {
      "plate": 1, "plateName": "", "seconds": 5025, "prepareSeconds": 360, "layers": 245,
      "bedType": "textured_plate", "filamentChanges": 4,
      "filaments": [{ "slot": 1, "type": "PLA", "color": "#00AE42", "grams": 14.88, "meters": 4.99, "cost": 0.37 }],
      "totalGrams": 14.88, "totalMeters": 4.99, "totalCost": 0.37,
      "warnings": [{ "level": "warning", "code": "", "message": "..." }]
    },
    "printer": { "preset": "...", "model": "...", "nozzles": [0.4], "filamentPresets": ["..."] }
  }
}
```

- `action` 喺 `command` 係個指令嘅 id，其他時候係空。
- `gcode.exported` 嘅 `slice.file` 係匯出檔案嘅名。
- 一次發送所有打印板嗰陣，`slice` 會將每塊切咗片嘅打印板加埋：`plate` 係 0，`allPlates` 係 true，`plateCount` 講有幾多塊；時間、層數、換墨水次數同每個槽嘅墨水都係總數；`bedType` 除非每塊都一樣，否則係空；每個警告都有佢嘅 `plate`。
- `send`，畀 `print.before_send` 用（要 `printer.read`）：

  ```json
  "send": {
    "plate": 1, "allPlates": false,
    "printerModel": "Bambu Lab X1 Carbon", "slicedForThisModel": true,
    "connection": "lan",
    "options": { "bed_leveling": "auto", "flow_cali": "off", "timelapse": "on" },
    "filaments": [
      { "slot": 1, "type": "PLA", "color": "#00AE42", "tray": "A1",
        "trayType": "PLA", "trayColor": "#00AE42", "remainPercent": 61 }
    ]
  }
  ```

  `connection` 係 `cloud` 或者 `lan`。`options` 係打印對話框顯示緊嘅選項同佢用緊嘅值。每個墨水係項目嘅一個槽，有切片時嘅類型同顏色；有對應嘅話，仲有打印機對嗰個槽嘅叫法（`A1`、`HT-A`、`Ext`）、打印機喺度報告嘅嘢，同剩幾多（打印機冇講就係 `-1`）；冇對應嘅 `tray` 係空。打印機自己嘅名、序號、地址同存取碼永遠唔會喺入面。
- `print`，畀 `print.finished` 用（要 `printer.read`）：`result`（`finished`、`failed` 或者 `cancelled`）、`printerModel`、`seconds`（Bambu Studio 見到佢行咗幾耐，或者 `null`）、`seenFromStart`（開始睇嗰陣已經行緊就係 false，即係 `seconds` 只係一部分）、`layers`（打印機報告嘅層數，唔知就係 0），失敗嘅打印仲有 `error`（打印機嘅代碼，八個十六進位數字）。開始睇之前已經完咗嘅打印永遠唔會報告，每次完結只報一次。打印機嘅身份只會用嚟比較有冇換機，永遠唔會送出。未有人答過擴充功能嘅同意提示之前，監察器連擴充功能資料夾都唔會讀，所以內置擴充功能仍然係第一次用先安裝。
- `model` 有 `plate`、`objects`（最多 200 個：`name`、`instances`、`triangles`、以毫米計嘅 `size`、`selected`）、`objectCount`、`selectedCount` 同 `triangles`。`model.imported` 嘅 `model` 淨係有加入咗嘅物件，仲有 `"imported": true`、`files`（載入咗嘅檔案名，永遠冇資料夾，最多 50 個）同 `"units": "mm"`。
- 克數同成本要靠墨水預設嘅密度同價錢；冇嘅話就係 0，成本會唔包括。

### 答覆

每個欄位都係可選；空物件 `{}` 都係有效答覆，乜都唔顯示。

| 欄位 | 掛鈎 | 權限 | 規則 |
| --- | --- | --- | --- |
| `apiVersion` | 全部 | | 有嘅話係 `1` |
| `notes` | 全部 | `notes.show` | 最多 10 個字串，每個最多 300 個字元，以一個通知顯示（發送打印之前就喺檢查視窗度顯示） |
| `checklist` | `print.before_send` | `send.warn` | 最多 20 行 `{"state": "pass" 或者 "warn" 或者 "fail", "text": ...}`，text 最多 200 個字元 |
| `block`、`blockReason` | `print.before_send` | `send.block` | `true` 同一個最多 300 個字元嘅原因 |
| `gcode` | `gcode.exported` | `gcode.comment` | `{"prepend": [...], "append": [...]}`，各自最多 20 行，每行最多 200 個字元，要用 `;` 開頭，唔可以有換行 |
| `file` | `command` | `file.save` | `{"name": ..., "text": ...}`：名最多 80 個字元，用 `.txt`、`.md`、`.json` 或者 `.csv` 結尾，唔可以有 `\ / : * ? " < > |`，文字最多 64 KB |

註解行會放喺 Bambu Studio 嘅標頭區塊之後（冇標頭嘅檔案就放喺最頭），同埋最後一行之後；每個擴充功能嘅行前面都有 `; added by extension <id> <version>`。檔案會先喺原檔旁邊寫成 `<name>.partial`，核對準確大細之後先放落原位；有任何問題，匯出咗嘅檔案都會保持原狀，並有通知講原因。

指令畀嘅檔案會打開儲存對話框，預先填好建議嘅名；你揀好位置之前乜都唔會寫。

### hello.ps1

```powershell
$utf8 = [System.Text.UTF8Encoding]::new($false)
$request = [System.IO.StreamReader]::new([Console]::OpenStandardInput(), $utf8).ReadToEnd() | ConvertFrom-Json
$minutes = [math]::Round($request.context.slice.seconds / 60)
$answer = @{ apiVersion = 1; notes = @("Hello: this plate takes about $minutes minutes.") }
$writer = [System.IO.StreamWriter]::new([Console]::OpenStandardOutput(), $utf8)
$writer.Write(($answer | ConvertTo-Json -Compress -Depth 5))
$writer.Flush()
```

Windows PowerShell 5.1 會用系統嘅 ANSI 代碼頁讀冇位元組順序標記嘅指令碼，所以指令碼要保持 ASCII，或者儲存做有位元組順序標記嘅 UTF-8。

任何語言都係咁用，例如裝咗 Python 就用 `"run": {"windows": "py.exe -3 hello.py"}`，或者用你自己嘅程式：`"run": {"windows": "hello.exe"}`，`hello.exe` 放喺資料夾入面。

### 失敗

| 結果 | 意思 |
| --- | --- |
| 已答覆 | 有效答覆 |
| 超過時間上限 | 喺時間上限被結束 |
| 答覆超過上限 | 佢寫多過最大答覆 |
| 唔係有效 JSON | 佢嘅輸出唔係一個 JSON 物件 |
| 犯咗規則 | 有效嘅 JSON 答覆，但犯咗上面一條規則；會講係邊條，例如 `not-allowed:gcode.comment` 或者 `not-comment` |
| 以結束代碼 N 停咗 | 佢用 0 以外嘅代碼結束 |
| 失敗 | 開唔到佢，例如 `program not found` |
| 你停咗佢 | **停止**，或者 Bambu Studio 關閉；永遠唔算失敗 |

失敗會出一個通知，有**打開擴充功能**。連續失敗夠設定嘅次數，擴充功能就會自己關咗，呢頁會講明；再開佢就會重新計數。

## 儲存嘅值

| 喺邊度 | 乜嘢 |
| --- | --- |
| `%APPDATA%\BambuStudio\extensions\<folder>` | 裝咗嘅擴充功能；內置嘅仲有 `.builtin.json` |
| `%APPDATA%\BambuStudio\extension-state\consent.json` | 每個擴充功能：你答咗邊個版本、允許咗乜、開定關、連續失敗次數 |
| `%APPDATA%\BambuStudio\extension-state\settings.json` | 主開關、限制同你移除咗嘅內置擴充功能 |
| `%APPDATA%\BambuStudio\extension-state\runs.jsonl` | 最近 500 次執行：時間、擴充功能、掛鈎、打印板或者檔案名、秒數、答覆大細、結果。永遠唔記送出或者答覆嘅內容 |

## 檢查

- `tests/extension_core.test.mjs`：清單檔同拒絕、請求同答覆嘅權限執行、同意、限制、執行紀錄、發送打印之前嘅判定同真實程序。佢嘅反向建置永遠唔會關失敗嘅擴充功能，或者放走阻止咗嘅打印，必須失敗。
- `tests/extension_host.test.mjs`：內置擴充功能嘅安裝、只喺冇改過嗰陣更新、移除咗就保持移除；加資料夾；重複同壞咗嘅清單檔；同意；主開關；限制；真實執行、令擴充功能自動關嘅失敗，同埋停止執行。佢嘅反向建置會覆寫改過嘅內置擴充功能，必須失敗。
- `tests/extension_context.test.mjs`：請求嘅各部分（包括 `send` 部分同所有打印板加埋）、樣本同 G-code 註解行（放喺標頭之後、保留換行方式、拒絕、大檔案）。佢嘅反向建置將啲行放喺標頭上面，必須失敗。
- `tests/extension_before_send_wiring.test.mjs`：打印對話框喺自己嘅檢查之前、而且淨係為切咗片嘅打印板問擴充功能；發送資料永遠唔讀打印機嘅序號、名、地址或者存取碼；檢查排喺所有等緊嘅執行前面，結果交畀佢嘅視窗；離開視窗會停咗啲執行；每個檢查喺壞咗嘅副本上都必須失敗。
- `tests/extension_builtins.test.mjs`：內置清單檔同指令碼；喺 Windows 上每個內置擴充功能都會經應用程式自己嘅程式碼喺 Windows PowerShell 下真係行一次，壞咗嘅副本必須被捉到。
- `tests/extension_import_wiring.test.mjs`：載入檔案會喺載入返嚟、有物件加入之後先行 `model.imported`，冇其他地方會叫內部嘅載入器，請求淨係有加入咗嘅嘢；每個檢查喺壞咗嘅副本上都必須失敗。
- `tests/extension_print_watch.test.mjs`：監察器喺啟動之後開始、關程式嗰陣停；冇擴充功能開咗 `print.finished` 就乜都唔睇；經旁述功能測試過嘅轉變偵測器，每次完結只報一次；永遠唔送打印機身份；每個檢查喺壞咗嘅副本上都必須失敗。
- `.github/workflows/extensions-windows.yml` 喺託管嘅 Windows 機器上用 MSVC 行晒呢七個。

## 未做

每個宣告咗嘅掛鈎都會行。打印隊列入面已經有嘅工作開始打印、打印機上已經有嘅檔案、同時發送去幾部打印機，同埋發送去打印機儲存空間，都唔會行 `print.before_send`。`print.finished` 淨係睇裝置頁揀咗嗰部打印機，亦講唔到一次打印用咗幾多墨水。仲未有每個擴充功能自己嘅設定，亦未有簽署過嘅擴充功能。
