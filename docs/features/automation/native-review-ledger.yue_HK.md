# 私人原生操作紀錄

[English](native-review-ledger.md)

`scripts/md3/review-ledger.py` 只核對已記錄嘅原生觀察資料。佢唔會啟動應用程式、
輸入、截圖、改圖、發佈檔案或者宣告執行驗收通過。呢項只係日後獲授權檢查嘅
**原始碼準備**，唔代表檢查清單已經執行。

現有[本機檢查工具](local-native-review.yue_HK.md)只處理初始視窗，之後結束自己
擁有嘅程序，唔會收集操作步驟。進一步操作仍然需要另外審核同授權嘅原生輸入支援。
紀錄工具唔會補上呢項能力，亦唔會喺程序結束後延長工作階段。

## 私人執行同保留資料

使用已經有 Pillow 嘅 Python 環境；工具唔會安裝相依套件。輸出必須係作業系統
暫存根目錄直接之下、未存在而且由本次工作擁有嘅 `bambu-ledger-` 目錄：

```powershell
$EvidenceRoot = Join-Path ([IO.Path]::GetTempPath()) ('bambu-ledger-' + [guid]::NewGuid())
& $Python scripts/md3/review-ledger.py --input $PrivateObservations --evidence-root $EvidenceRoot
```

`$PrivateObservations` 指向現有私人 JSON。儲存庫內嘅輸出位置會被拒絕。
`ledger.json` 保留觀察、原本建置及工作階段收據、原生檢查結果，同原檔路徑及雜湊。
原檔留喺原本工作階段目錄，唔會被改寫。**必須同時保留原檔同建置檔案**；呢個係
參照紀錄，唔係可攜封存。之後原檔消失或者改變，重新核對就會失敗。工具唔會讀取
或者複製個人設定目錄。

標準輸出只含狀態、步數同未驗收／未授權發佈結果。無效輸入或輸出只會喺 stderr
寫一般失敗訊息並以 2 結束，唔會印出私人路徑或者語義內容。非空而一致嘅紀錄以 0
結束；空紀錄會儲存 `incomplete` 並以 2 結束。0 只代表資料一致性，唔代表介面驗收。
輸入、檔名、收據、探針、截圖同語義內容仍然係私人資料，通過私隱檢閱都唔等於可發佈。

## 輸入格式

未知欄位會被拒絕。檔案參照只有 `path` 同小寫 `sha256`，路徑必須絕對並指向普通
檔案，唔接受符號連結或者重新解析點。JSON 上限 16 MiB、探針每份 16 MiB、PNG 每張
64 MiB。

每個 JSON 同 NDJSON 輸入口都會拒絕任何層級嘅重複物件鍵、非標準 `NaN`／
`Infinity` 常數，同指數溢位成無限大嘅數值，包括輸入紀錄、建置及工作階段收據、
automation 建置身分，同初始及每步探針。JSON 讀取本身最多只要求上限加一個
位元組，超額就喺解碼之前拒絕，唔會因為早前檔案大小檢查通過而無限讀取增長中嘅
檔案。建置及 companion JSON 上限 1 MiB；輸入、工作階段 JSON 同**所有探針，
包括 `shell.jsonl`，上限都係 16 MiB**。紀錄工具獨立載入嘅原生驗證器會用同一
嚴格讀取器處理內層 companion；共用啟動器原始碼冇改動。輸出序列化亦會拒絕
非有限數值。雜湊、大小檢查同最後重新核對仍然必須通過。

| 紀錄 | 必要欄位 |
| --- | --- |
| 根物件 | `schemaVersion: 1`、`kind: "local-native-interactions"`、`producer`、`sourceCommit`、`buildReceipt`、`session`、`steps` |
| `buildReceipt` | 現有第 1 版 `local-root-build` 收據嘅檔案參照 |
| `session` | `id`、`review` 參照；ID 必須係暫存根目錄直接之下、包含 `review.json` 嘅實際 `bambu-local-review-*` 名稱 |
| 每一步 | `id`、由 1 開始嘅 `sequence`、`status: "observed"`、`sourceCommit`、`buildReceiptSha256`、`sessionId`、`pid`、`action`、`pre`、`post` |
| `action` | `method` 為 `native-keyboard` 或 `native-pointer`、實際觀察到嘅可存取目標名稱 `target`、`atUtc` |
| `pre`／`post` | `atUtc`、非空實際 `semanticState`、正整數 `hwnd`、`tuple`、`probe` 檔案參照、`capture`、`privacy` |
| `tuple` | `language`、布林 `dark`、`density`、實測 `dpiScale`、含整數 `w`／`h` 嘅 `client`、觀察到嘅 `motion`（`normal` 或 `reduced`） |
| `capture` | `file` 參照及 `reply`；後者只有 `rendered_ok`、`mode`、`window_hwnd`、`path`，保留相應 Lowlevel 原始回覆值 |
| `privacy` | `status: "reviewed-safe"`、非空 `reviewer`、`reviewedAtUtc`；係人手檢閱聲明，唔會由檔名推斷 |

每次實際輸入前後都要記錄真實語義狀態，唔係預期結果。空內容同 `planned`、
`pending`、`unknown`、`unverified`、`todo` 呢啲佔位文字會被拒絕。ID 唔可重複，
序號必須連續。輸入時間必須嚴格喺前後觀察之間；所有 UTC 時間以 `Z` 結尾，
唔可早過建置完成或者遲過現在。私隱檢閱唔可早過觀察。下一步完整 `pre` 必須等於
上一步 `post`，包括檔案參照，工具唔會自動補上缺失轉換。前後探針及 PNG 必須用
唔同路徑保留，唔接受一份檔案冒充兩次觀察。

## 沿用合約同限制

工具喺讀取前後都呼叫 `local-native-review.py` 建置驗證器，保留根建置入口成功、
24 小時有效期、乾淨來源及樹身分、transcript 次序、產物雜湊同 automation companion
檢查。活動中或者已改變嘅建置唔會被改稱為觀察目標。初始工作階段收據必須包含一致
建置收據及目前 driver 雜湊、已觀察 PID／shell、收到嘅探針及已確認結束，唔可有
失敗紀錄。`shell.jsonl` 會按 PID、HWND 同目錄衍生嘅工作階段標記重新核對。

每步探針都用同一原生驗證器：完整 `end`、一致 PID／標記、可見目標、正數客戶區
尺寸同有效 DPI。本版刻意只支援現有 **英文／淺色／舒適密度**；其他語言、深色同
緊密密度要先有經審核嘅原生參數合約。`motion` 係觀察者聲明，現有探針唔會報告
實際動態設定。唔會由外層視窗矩形推斷最小尺寸、DOM 或 renderer 內部尺寸。

每張圖必須係現有、非單色 PNG，尺寸符合實測客戶區、Lowlevel 視窗回覆同 HWND
一致、SHA-256 一致。所有每步觀察檔案都要留喺工作階段目錄，最後會再核對雜湊。
原生問題旗標同其他可見擁有視窗會保留，但唔會自動變成視覺驗收結果。

呢啲係本機、無簽署嘅觀察者紀錄。雜湊只綁定位元組，唔證明聲明屬實；提交嘅
Lowlevel 回覆唔係已驗證身分嘅收據，非單色圖都唔能夠證明真實截圖來源、語義、
私隱、工作階段時間或者實際輸入。觀察者必須檢視真實圖像、審核敏感內容、保留
原始來源並只作獲授權輸入。唔可用合成圖當真實證據；單元測試嘅合成圖只測拒絕
邏輯。`runtimeAcceptance` 同 `visualAcceptance` 永遠係 `unverified`，
`publication` 永遠係 `not_authorized`。發佈仍需獨立私隱、來源審核及授權。

集中測試指令：`python -B -m unittest discover -s scripts/md3/tests -p test_review_ledger.py -v`。
測試只用合成檔案同注入嘅 Git 讀取器，唔會執行應用程式，亦唔證明真實操作完成。
