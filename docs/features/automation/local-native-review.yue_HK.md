# 本機初始介面檢查

[English](local-native-review.md)

`scripts/md3/local-native-review.py` 會核對已完成嘅本機根目錄建置，並可檢查
初始原生介面。呢條路線同託管安裝程式驗證分開，唔會改動託管限制，亦唔會
偽造安裝收據。目前只完成非視窗測試。實際執行仍須明確授權，同埋成功而且
身分吻合嘅建置。

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
檢查同清理完成。目標先以暫停狀態建立，加入容器後先恢復執行，桌面固定係
`WinSta0\Default`。目前輸入桌面本身必須已經係 `Default`，路線唔會切換
桌面。帶命令殼特殊字元嘅啟動路徑會被拒絕。目標只接收
`--datadir <fresh-profile>`。

工作程序上限係 225 秒，Lowlevel 工具上限係 270 秒，外層 CLI 上限係
300 秒。中斷會寫入所屬停止標記，由監察程序退出並關閉自己嘅容器控制代碼。
逾時或者缺少報告只可標示清理未驗證，唔可以當成功。正常清理會終止所屬
容器、等待原程序，並確認容器內活動程序數為零。唔會按視窗標題或者程式
名稱終止程序。

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
本機建置類型同執行檔雜湊亦要吻合。文字記錄要先有來源鎖定，再有呢個
payload 嘅 build-only 完成記錄。後來未完成嘅呼叫唔可以借用之前嘅成功。
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
- `review.json`：分開記錄啟動、探測、截圖同清理結果。
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
payload 漂移、未完整記錄、錯誤或含糊視窗身分、部分啟動、逾時、先加入
容器後恢復、所屬清理、完整探測同分開截圖結果。測試唔會啟動產品、對真實
視窗操作 Lowlevel、核實真實建置收據，亦唔證明 Windows 執行中容器行為。
實際檢查仍然待辦。
