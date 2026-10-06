# 離線原生互動計劃

`scripts/md3/review-interaction-plan.py` 只驗證
[九個邊界的檢查準備](../../../design/workflow-refresh/built-review-queue.md)，
不會啟動程式、輸入、截圖、匯出、執行回呼或操作打印機。
通過只代表計劃符合有限結構，不代表畫面已到達、已觀察或已獲接納，亦不提供執行授權。

## 來源及格式

```powershell
& $python scripts/md3/review-interaction-plan.py --repository . --source-commit $reviewedCommit --plan $privatePlan
```

完整的 `$reviewedCommit` 必須經獨立審閱，不能直接相信計劃提供的值。
頂層只接受 `schemaVersion`（整數 `1`）、`kind`（`native-review-preparation`）、
`sourceCommit`、`contractSha256`、`steps`。雜湊表必須包含以下三個路徑，
值為該提交內原始檔案的 SHA-256：

- `design/workflow-refresh/built-review-queue.md`
- `design/workflow-refresh/implementation-scopes.json`
- `design/workflow-refresh/manifest.json`

工具用 `git show <commit>:<path>` 讀取固定路徑，不採用工作目錄內的修改。
這只證明資料身份，並非簽署、編譯證據，亦不證明另一個執行檔具有相同來源。
現有的本機編譯來源驗證仍然必須通過。

每步只接受 `boundary`、`state`、`action`、`timeoutMs`，例如：

```json
{"boundary":"shared-control-callers","state":"menus/keyboard","action":"menu-next","timeoutMs":1000}
```

邊界取自 `remainingVisualCoverage`，狀態必須同時出現在該邊界的檢查文字及
參考清單中。它們是待檢查狀態，不是原生控制項識別碼，也不是已觀察的聲明。
只支援現有順序的九個邊界。

## 有限動作

`observe-native` 可要求觀察所屬邊界的狀態。日後執行時，必須先獨立確認
該狀態已存在於當前擁有的程序中；原生外框量度不能證明 DOM 或 OpenGL 內部。

`menu-next`、`menu-previous`、`menu-dismiss` 只適用於 `shared-control-callers`
的 `menus/submenu`、`menus/disabled`、`menus/keyboard`。它們分別只表示一次
Down、Up、Escape，不能表示啟動選項、Enter、Space、點擊或打開選單。
日後必須先證明實際選單已開啟、焦點控制項身份及安全取消路徑。

Lowlevel 的 `win_send_keys` 接受明確的 `hwnd` 及 `keys`；指定的句柄就是目標，
不會自動找到有焦點的子控制項。送出按鍵訊息不代表 wxWidgets 已處理。
目前沒有經審閱的本機轉接器可驗證這條路徑，現有 `cheap()` 允許清單亦沒有
此工具。因此這些動作仍然只是準備，這次修改不會擴大執行能力。

計劃不接受 HWND、PID、座標、視窗標題、任意按鍵、輸入文字、命令、回呼、
匯入匯出路徑或成功聲明。不明欄位、重複 JSON 鍵、非有限數值及執行收據均被拒絕。
上限為 64 KiB UTF-8 JSON、1 至 64 步、每步整數 100 至 2,000 毫秒，
總和最多 30,000 毫秒。時間只是日後執行上限，不是等待或重試。
命令列診斷不會反射被拒絕的內容；私人計劃毋須提交。

## 日後接駁位置

[現有本機檢查](local-native-review.yue_HK.md) 只處理初始主畫面，並在
`inspect_shell()` 的 `finally` 關閉擁有的程序工作。返回的收據不會保留活動工作階段。
日後經審閱的轉接器必須在同一個擁有生命週期內執行，完成編譯來源、設定檔及
視窗身份檢查後，最終來源複核及程序結束前接駁，並保留監察時限與清理保證。

每步前重新檢查程序存活、PID／工作擁有權、視窗、焦點及實際狀態。
目標遺失、停用、改變或不明確便停止。每步後重新量度，保留真實證據；
不能把驗證輸出當成執行收據，也不能把傳輸成功當成介面成功。
需要另外審閱實作、授權及測試；本工具沒有執行入口，`--execute` 會被拒絕。

打開控制項、修改資料、建立模型或其他本機測試資料、匯入、改變大小、主機顯示設定、
截圖、登入、裝置操作、切片、打印、傳送、下載、匯出及提交均不支援。
例如 `print/mapping` 只要求量度另外獲授權到達的狀態，不會建立切片或裝置資料。

## 驗證

```powershell
& $python -m unittest discover -s scripts/md3/tests -p test_review_interaction_plan.py -v
```

八項針對性測試包含九個邊界的實際已提交例子、選單範圍、來源與雜湊不符、
不明輸入及執行聲明、解析與時限，以及離線命令列。沒有啟動程式，亦沒有運行畫面證據。
