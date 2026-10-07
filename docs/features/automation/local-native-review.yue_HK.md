---
translation-of: local-native-review.md
source-sha256: 80497823bdec7c066b9ade6520421ad118a069b75f1a2a52b5917858367c6492
review-status: agent-drafted
---

# 本機初始介面檢查

[English](local-native-review.md)

`scripts/md3/local-native-review.py` 會核對已完成嘅本機根目錄建置，並可檢查
初始原生介面。呢條路線同託管安裝程式驗證分開，唔會改動託管限制，亦唔會
偽造安裝收據。一次獲授權嘅可見執行喺檢查前已退出，當時版本嘅驅動程式
冇保留目標退出碼，啟動原因仍未確立。退出觀察修正只完成非視窗測試。
之後實際執行仍須明確授權，同埋成功而且身分吻合嘅建置。

## 執行範圍

預設只核對來源，唔會啟動應用程式：

```powershell
& $Python scripts/md3/local-native-review.py `
  --producer $Producer --build-receipt $Receipt --source-commit $SourceCommit
```

獲授權之後，先可以明確揀可見桌面：

```powershell
& $Python scripts/md3/local-native-review.py `
  --producer $Producer --build-receipt $Receipt --source-commit $SourceCommit `
  --lowlevel-cli $LowlevelCli --execute --desktop visible
```

只有截圖亦獲授權時先加 `--capture`。路線只會透過 Lowlevel `screenshot`
擷取所屬 HWND 嘅客戶區，唔會擷取整個螢幕。以上變數喺執行時指向現有
Python、建置工作目錄、收據同 Lowlevel CLI，唔係使用者平日用開嘅設定目錄。

可見路線使用 Lowlevel 已記錄嘅 `run_command`，透過 `--json` 傳入
`command`、`shell`、`cwd` 同 `timeout`。隱藏嘅命令外層會執行同一份腳本
入面有時限嘅工作程序。工作程序保留自己嘅匿名、關閉即終止程序容器，直到
檢查同清理完成。目標透過 `PROC_THREAD_ATTRIBUTE_JOB_LIST` 建立嗰刻已經
以暫停狀態置於容器內，再恢復執行，桌面固定係
`WinSta0\Default`。目前輸入桌面本身必須已經係 `Default`，路線唔會切換
桌面。帶命令殼特殊字元嘅啟動路徑會被拒絕。目標只接收
`--datadir <fresh-profile>`。

工作程序上限係 225 秒，Lowlevel 工具上限係 270 秒，外層 CLI 上限係
300 秒。中斷會寫入所屬停止標記，由監察程序退出並關閉自己嘅容器控制代碼。
逾時或者缺少報告只可標示清理未驗證，唔可以當成功。正常清理會終止所屬
容器、等待原程序，並確認容器內活動程序數為零。唔會按視窗標題或者程式
名稱終止程序。

正常清理所屬程序之前，驅動程式會對確切目標程序控制代碼作零逾時等待。
只有控制代碼已收到結束訊號，而且 `GetExitCodeProcess` 成功，
`targetExit.status` 先會係 `exited`。報告保留無符號 32 位元 `exitCode`、
十六進位 `exitCodeHex`、UTC 觀察時間同 `observedBeforeTeardown: true`。
控制代碼已收到結束訊號之後，真實退出碼 259 亦會保留。未收到訊號就記錄
`active`，唔附退出碼；呢個只係當刻觀察，唔保證緊接住會發生乜嘢。
控制代碼不可用、等待結果異常或查詢失敗會記錄 `unavailable`，唔會虛構
退出碼。觀察失敗唔會阻止所屬清理。如果冇傳回工作階段，目標維持
`not_observed`；工作程序中斷而冇有效證據就記錄 `unavailable`。
外層嘅 `workerExitCode` 同清理時提供嘅終止碼，永遠唔會代替目標結果。
退出碼係診斷證據，唔等於已診斷啟動原因。

桌面操作只包括列出視窗同可選嘅指定視窗截圖。現有
`send-layout-probe.py` 只發出讀取版面要求，唔會收到選單或導航命令。
原生 PID 同視窗類別會先篩走其他應用程式，無關視窗資料唔會寫入報告。
有多個可能主介面時會停止。新設定使用英文、淺色、舒適密度、減少動態效果，
關閉更新檢查、單一執行個體轉送同提示。路線唔會操作登入、導航、匯入模型、
更新、硬件或列印。啟動本身仍可能顯示設定精靈或產品網絡內容，呢條路線
唔保證阻止所有啟動網絡要求，亦唔會自動關閉意外出現嘅介面。

## 真實建置收據

目前根目錄建置程式只寫追加式文字記錄，唔會產生呢份 JSON。建置負責人
必須記錄實際 `build.bat /s` 呼叫同最終結果，唔可以見到執行檔就補填成功。
呢份收據係受信任本機觀察記錄，唔係簽署證明。收據同原始記錄必須保留喺
私人或忽略目錄，唔可以放入公開來源記錄。

完整 JSON 結構見[英文文章](local-native-review.md#required-build-receipt)。
必要欄位包括 `schemaVersion: 1`、`kind: local-root-build`、真實
`invocationId`、`entrypoint: build.bat`、`arguments: [/s]`、來源提交及樹雜湊、
建置前後乾淨狀態、UTC 開始及完成時間、整數 `exitCode: 0`、根入口雜湊、
文字記錄絕對路徑及雜湊，以及 `install-dir` 同以下四份檔案嘅 SHA-256：

- `bambu-studio.exe`
- `BambuStudio.dll`
- `automation/bambu-automation.exe`
- `automation/build-identity.json`

建置目錄必須仍然乾淨，來源提交同樹必須一致。自動化工具自己記錄嘅來源、
本機建置類型同執行檔雜湊亦要吻合。驗證器會選擇最後追加嘅 PowerShell
記錄工作階段，即使佢喺建置程式開始或來源鎖定之前已中斷亦一樣。
該工作階段必須依次只有一個實際呼叫開始、相符來源鎖定、呢個 payload
嘅 build-only 完成，以及最終工作流程成功記錄。四者嘅建置程式 UTC
時間戳必須按時間不倒退，並位於收據開始／完成區間內。
`Write-BuildLog` 只記錄整秒，所以收據開始時間按相同精度比較；記錄
標頭／結尾嘅本地時間唔會當作 UTC。成功之後只可以有完整關閉結尾。
之後另一來源嘅呼叫、來源鎖定前中斷，或重新收集雜湊，都唔可以借用
之前完成記錄。重複、冇時間戳、次序錯誤、區間外時間、缺少最終成功，
或者追加失敗文字都會被拒絕。
完成時間必須少於 24 小時，未來或未完成時間、非零或未知結果、改動檔案、
缺漏記錄同重新導向路徑都會被拒絕。啟動前同檢查後都會再核對。呢啲檢查
只綁定列出嘅檔案，唔等於驗證所有外部執行環境或作業系統狀態。

## 證據同版面

每次獲授權執行，都會喺作業系統暫存根目錄建立全新
`bambu-local-review-<random>`，再建立帶現有校驗格式嘅
`profile/BambuStudio.conf`，唔會覆寫舊設定。證據唔會自動移入版本控制：

- `request.json`：有限範圍要求同明確桌面選擇。
- `shell.jsonl`：原生視窗、客戶區、最小及最佳尺寸、版面記錄。
- `shell.png`：可選嘅所屬視窗原始截圖。
- `review.json`：分開記錄啟動、目標退出觀察、探測、截圖同清理結果。
- `wrapper-failure.json`：中斷或未完整完成，保持未驗證。

探測必須有最後嘅 `end` 記錄、正確 PID 同標記、指定語言主題密度、正數
客戶區尺寸同有限正數 DPI。報告保留原生版面問題及其他可見所屬頂層視窗。
收到探測記錄唔等於證明主介面冇被遮擋。路線唔會調整視窗或 DPI，亦唔會
將實際尺寸改寫成 `1200x800` 或最小尺寸。產品最小值仍然係
`max(1000,76*em)` 乘 `max(600,49*em)`，尺寸測試要另行獲授權。

截圖必須符合 PrintWindow 成功、正確目標及路徑、有效 PNG、客戶區尺寸
一致同非單色像素。但仍然標示 `privacy: unreviewed` 同
`visualAcceptance: unverified`。初始介面檢查唔涵蓋九個餘下視覺邊界、
完整語言主題縮放矩陣、內嵌版面、渲染工具內部、互動、設計一致性或
1,204 項功能責任。56 塊結構參考板唔係截圖。

## 聚焦驗證

```powershell
& $Python -m unittest discover -s scripts/md3/tests -p test_local_native_review.py -v
```

測試只用暫存合成收據、模擬原生呼叫同合成 PNG，覆蓋來源反例、來源及
payload 漂移、未完整記錄、錯誤或含糊視窗身分、部分啟動、逾時、建立嗰刻已經
歸入容器、所屬清理、完整探測同分開截圖結果。測試唔會啟動產品、對真實
視窗操作 Lowlevel、核實真實建置收據，亦唔證明 Windows 執行中容器行為。
成功嘅實際檢查仍然待辦。

退出觀察修正通過呢份針對性檔案全部 32 個方法，包括六個新增方法，覆蓋
已結束程序嘅零、非零、259 及高位元退出碼、活動及不可用狀態、活動查詢
失敗、清理前觀察，以及觀察拋出例外之後仍然清理。舊驅動程式喺新增
檢查回歸中出現兩項缺少證據錯誤。工作程序逾時測試亦確保外層結果唔會
變成目標退出碼。呢啲檢查唔會啟動產品，亦唔會補回之前可見執行遺失嘅退出碼。

記錄綁定修正執行咗 15 個針對性 `ReceiptTests` 方法，冇原生執行。
舊驗證器喺新負面案例中有 16 項斷言失敗；修正後 15 個方法全部通過。
涵蓋之後不同來源同來源鎖定前呼叫、四個時間戳界線、標記次序、最終
關閉、較早工作階段、UTF-8／BOM／UTF-16 同帶小數秒觀察時間。
另一次唯讀相容性檢查，將來源 `a28944e3c14b2066ee63d14151c8aca23066d743`
現有不可變成功收據，交畀完整修正版驗證器並通過。檢查冇啟動任何產品，
只證明收據相容性，唔代表介面驗收。
