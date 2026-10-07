---
translation-of: windows-release-supply-chain.md
source-sha256: da329019b2ccd790331fdb70b294d28ea0c95ea6c6cb43a10943bc940733e754
review-status: agent-drafted
---

> 英文原文：[Windows CI and release supply chain](windows-release-supply-chain.md)

# Windows CI 同埋 release 供應鏈

## 觸發同埋發佈政策

`.github/workflows/build_all.yml` 係 fork 嘅 Windows 構建同埋發佈工作流。每一次分支推送同埋手動觸發都會構建 Windows 候選版本；一個針對 `main` 嘅 pull request 會執行構建而唔發佈。一個輕量級嘅路徑分類工作只係資訊性嘅，所以單純嘅文件推送唔會被無聲咁跳過。分支專用推送過濾器同埋一個顯式嘅標籤警衛會防止 release 標籤遞迴地開始另一次構建。

每一次成功嘅非 pull request 分支推送或手動觸發都會發佈一個唯一標籤的、非草稿嘅 release。標籤包含應用版本同埋工作流執行編號。一次重新執行會收斂到相同嘅標籤，而唔係建立一個重複嘅。release 工作會喺發佈草稿之前驗證確切嘅 Squirrel 資産、原始碼提交元數據、校驗和、空嘅 PE 安全目錄（未簽署嘅 Setup.exe）、feed 索引、完整軟件包、SBOM 同埋 GitHub 資産摘要。除咗存放構建快取嘅草稿（呢個草稿永遠唔會發佈，詳見[構建快取](#構建快取)），構建工作唔會建立快取預發佈或者任何其他次要嘅 GitHub Release。

發佈工作會一次執行一個，每一個都喺發佈前立即決定「最新版本」。預設分支構建會喺佢嘅提交比目前最新版本嘅提交更新時（或者係同一提交重新構建）變成最新版本。一個完成得遲嘅舊構建會喺佢嘅標題中帶住「(superseded main build)」發佈並保持非最新狀態，而其他分支嘅構建都保持非最新狀態。構建執行時分支可能已經向前移動；最新版本，同埋已安裝版本讀取嘅更新來源，仍然會向前移動。

發佈並行控制使用 `queue: max` 同 `cancel-in-progress: false`：一個工作執行中，最多 100 個等候，超額請求會被取消。先入先出指加入佇列嘅次序，唔係派發或來源提交次序，所以仍然要比較最新來源。詳見 [GitHub 並行控制](https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/control-workflow-concurrency)。

舊有預設只保留一個等候工作，即使 `cancel-in-progress: false` 都一樣。例如執行 `37088514258` 已完成構建及上傳安裝器，但發佈工作 `111113206485` 未執行任何步驟就被取消，註記係 `Canceling since a higher priority waiting request for windows-release-Ding-Ding-Projects/BambuStudio exists`。

復原歷史執行之前，先確認構建成功、安裝器未過期，而且冇同一執行嘅活動嘗試。用 `gh run rerun RUN_ID --failed` 或 `gh run rerun --job JOB_ID`，唔好重跑已成功嘅構建。重試沿用原有安裝器、來源及標籤冪等行為。歷史重試使用原工作流程版本，仍可能受舊單一等候規則影響；先協調清空佇列，再核對發佈、來源、資產雜湊同工作最終狀態。重試限原執行後 30 日內，最多 50 次嘗試，資產是否仍可取得要另外確認。詳見[重跑工作流程及工作](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/re-run-workflows-and-jobs)。

## Windows 構建同埋軟件包邊界

可重用嘅構建會解析或重新構建依賴快取、配置同埋安裝生產原生 Release 負載、添加哈希釘選嘅 Mesa 軟件 OpenGL 後備、生成一個 CycloneDX 1.6 清單、並用承諾嘅 `scripts/windows/Invoke-SquirrelPackage.ps1` 打包負載。Squirrel.Windows 2.0.1 只係喺未被快取嗰陣先至從官方 NuGet 扁平容器 URL 下載，而且佢嘅軟件包 SHA-256 會喺提取前被檢查。

當前工作流會刻意將正確性同埋 UI 證據檢查保留為本地 release 操作員檢查，而唔係 Actions 測試工作。承諾嘅本地檢查依然可用，同埋會喺手動 release 或接受一個候選構建之前執行。一個工作流構建仍然會喺編譯器、依賴、SBOM 或 Squirrel 軟件包失敗時失敗。

## 構建快取

託管嘅 Windows 構建以前每次都要編譯差唔多所有原始碼。sccache 會包住編譯器，但用到預編譯標頭嘅編譯佢快取唔到，而差唔多每個原始碼檔都用緊預編譯標頭：喺執行 36631880242（`bb78abee1`）入面，佢收到 795 個編譯請求，快取咗 99 個，其餘 696 個快取唔到（`/Fp` 693 個，`/Yc` 3 個）。"Build slicer Win" 呢一步喺嗰次 80 分鐘嘅構建工作入面用咗 72 分鐘。所以而家構建會重用最近一次 `main` 構建嘅成個 Ninja 構建樹（連預編譯標頭），Ninja 只會編譯之後改過嘅嘢。

**放喺邊度。** 構建樹存喺草稿 release `build-cache-windows`，分成每份最多 1,500,000,000 位元組嘅 7-Zip 分卷（GitHub 接受最大 2 GiB 嘅 release 資產）。草稿永遠唔會觸發 release 事件，永遠唔會變成最新 release，資產亦可以隨時替換；而呢度已發佈嘅 release 係改唔到嘅。呢個草稿永遠唔會發佈；萬一佢被發佈咗，`Save-BuildCache.ps1` 會拒絕再寫入。`windows-build-latest.json` 指明而家用緊邊一組。每一組係 `windows-build-<commit>.7z.001`、`.002`、... 加上 `windows-build-<commit>.json`；後者係清單，記錄提交、執行編號、快取鍵、構建樹大小，同埋每一份嘅名稱、大小同 SHA-256。呢啲步驟用擁有者權杖（secret `TOKEN_GITHUB`）讀寫草稿；`build_all.yml`、`build_check_cache.yml` 同 `build_deps.yml` 用 `secrets: inherit` 將佢一路傳落去。

**編譯之前：還原。** `scripts/ci/Restore-BuildCache.ps1` 只會喺構建樹嘅快取鍵同今次執行嘅一樣先至用佢。快取鍵包括：MSVC 工具組（`VCToolsVersion`）、Windows SDK、CMake 同 Ninja 嘅版本、依賴快取嘅快取鍵、工作區路徑，同埋腳本嘅佈局版本。佢會先查磁碟剩餘空間，再下載各份，解壓之前逐份核對大小同 SHA-256。簽出（checkout）會將每個檔案嘅時間設做而家，咁 Ninja 就會重新構建晒所有嘢。所以每個受追蹤嘅檔案都先設做 2020-01-01，然後喺快取嗰個提交同今次簽出之間有分別嘅檔案（比較嘅係工作樹，所以早前步驟改過嘅檔案都計埋）再設做而家。目的檔保留原本嘅構建時間，所以重新構建嘅就啱啱好係改過嘅檔案，同埋 include 佢哋嘅嘢。揀 2020 而唔揀更早嘅日期，係因為 Windows 上嘅 Ninja 大約由 2001 年開始計時間。裝置頁面嘅打包檔係構建入原始碼樹嘅，而簽出冇呢啲檔案，所以會刪走佢嘅 stamp 檔，令打包檔重新構建。

**`main` 構建之後：儲存。** 只有推送到 `main` 而又成功嘅構建先會儲存，所以每次構建都係由 `main` 嘅構建樹開始。打包負載嘅同時，`scripts/ci/Save-BuildCache.ps1` 喺背景開始運行。佢係隱藏咁啟動，日誌由佢自己寫：如果用重新導向輸出嘅方式啟動，步驟完咗之後佢仲會霸住步驟嘅輸出（已經喺本機量度過），而 runner 會停止仲霸住已完成步驟輸出嘅程序。佢用快速 LZMA2 壓縮（`-mx=1`）將構建樹封存，但唔包 `resources` 連接點（junction）、除錯資料庫，同埋裝置頁面嘅套件儲存庫。佢上載各份同清單，再由 GitHub 讀返每一份嘅大小；只有未有更新嘅執行將 `windows-build-latest.json` 指向自己嗰組，先會更新呢個檔案。佢只保留最新三組；冇清單嘅分卷滿兩個鐘先會刪，所以仲上載緊嘅執行唔會冇咗自己嗰啲。如果磁碟剩餘空間唔夠構建樹大小加 10 GB，佢唔會開始。工作嘅最後一步最多等佢 30 分鐘，然後印出佢嘅日誌。

**佢可以令構建快啲，但永遠唔會令構建失敗。** 冇權杖（例如由 fork 發出嘅 pull request）、未有快取、快取鍵唔同、有一份損壞或者唔見咗、磁碟空間唔夠，或者構建樹已經存在，都只會變成由零開始構建，同以前一樣，原因會寫喺警告入面。還原只會刪走佢自己解壓出嚟嘅構建樹。如果還原咗嘅構建樹配置失敗，構建步驟會刪走佢，再由零開始配置。儲存失敗只係一個警告。想強制由零開始構建，就喺推送嗰個提交嘅訊息入面寫 `[cold build]`：咁會跳過還原，而 `main` 構建照樣會儲存一個新嘅構建樹。改咗兩個腳本入面嘅佈局版本，就會作廢所有現有嘅組。

構建時間戳記（`SLIC3R_BUILD_TIME`、`SLIC3R_BUILD_TIME_UTC`）而家放咗喺 `libslic3r_build_time.h`，只有日誌、「關於」對話框同啟動畫面嘅日期會 include 佢。差唔多每個原始碼檔同預編譯標頭都會 include `libslic3r_version.h`；一個每次配置都會變嘅值放喺嗰度，就會令每次構建嘅每個目的檔都過期，有冇快取都一樣。

第一次託管執行之前，已經喺本機檢查過。用一個以資料夾代替 release 嘅替身 `gh`、真正嘅 7-Zip，同一個有四個提交嘅臨時儲存庫，將兩個腳本行一次，34 項檢查全部通過：每份都唔超過上限、熱還原嘅時間戳記準確、指標永遠唔會倒退、刪到只剩三組，同埋每種退回情況都唔會留低構建樹。另外用一個 Ninja 檔案（步驟只係寫檔同印出 MSVC 嘅 include 提示，冇用編譯器），確認咗 7-Zip 來回之後三件事：目的檔嘅時間準確還原到 100 ns；冇嘢改過就乜都唔會重新構建；改咗一個原始碼檔或者標頭，就只會重新構建佢嘅目的檔同連結。再用一個模仿 runner、將步驟輸出讀到尾嘅替身，證明咗點解儲存要隱藏咁啟動：用重新導向輸出啟動嘅子程序會一直霸住嗰個輸出，直到佢做完，即係步驟結束之後 20 秒；而隱藏啟動、自己寫日誌嘅子程序就會令輸出同步驟一齊關閉。

喺託管執行度量到嘅結果：

| 執行 | 提交 | 構建樹 | 編譯步驟 | 構建工作 | 編譯請求 |
| --- | --- | --- | --- | --- | --- |
| [36631880242](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36631880242) | `bb78abee1` | 冇（未有快取） | 71 分 53 秒 | 80 分 16 秒 | 795 |
| [36645906111](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36645906111) | `84b96e720` | 由零開始構建，儲存咗第一組 | 55 分 52 秒 | 冇記錄 | 冇記錄 |
| [36739933076](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36739933076) | `eff5fe381` | 由 `84b96e720` 還原，之後改咗 23 個檔案 | 3 分 4 秒 | 12 分 44 秒 | 10 |

第一組係一棵 6,786,436,758 位元組嘅構建樹，壓成一份 672,688,710 位元組。熱構建嗰次嘅還原步驟用咗 68 秒，佢嘅通知寫住「Build cache: restored the tree built from 84b96e720b8516895bf6b74b0429f46592b3639d (run 633); 23 files changed since.」。佢發佈咗 `md3-v176`，再將自己棵樹儲存做下一組（run 634，一份）。一次熱構建只係一個量度：改一個好多原始碼都 include 嘅標頭，仍然要全部重新編譯，所以之後某一次可以比今次耐好多。

## 負載 DLL

`BambuStudio.dll` 會載入 OpenCascade (`TK*.dll`)、FFmpeg (`avcodec-61.dll`、`avutil-59.dll`、`swscale-8.dll`、`swresample-5.dll`)、GMP 同埋 MPFR、FreeType 同埋 `WebView2Loader.dll`，同埋佢同埋 `bambu-studio.exe` 啟動器都會用 Visual C++ 執行階段(`MSVCP140`、`VCRUNTIME140`、`CONCRT140`)。呢啲都必須隨附喺負載裏面：

- `bambustudio_copy_dlls`(頂層 `CMakeLists.txt`)會從依賴項字首複製依賴項 DLL，而 `src/CMakeLists.txt` 會安裝佢返回嘅清單。無論係 multi-config 產生器(Visual Studio)定係 single-config 嘅(Ninja，就係工作流用嗰個)，都會被調用。Release `md3-v143` 就係喺得返 Visual Studio 分支先至調用佢嗰陣構建嘅：佢嘅負載得返 `BambuStudio.dll` 而已，`LoadLibrary` 失敗咗，錯誤 126(缺少一個依賴項)，啟動器喺應用寫到日誌之前就退出咗，返回碼係 -1。
- `InstallRequiredSystemLibraries` 會喺應用旁邊安裝 Visual C++ 執行階段(應用本地部署)，所以一部 Windows 機器就算冇 VC++ 可轉散發套件都能夠啟動佢。
- "Verify the payload carries every DLL the app imports" 呢一步會執行 `scripts/ci/check_payload_imports.py` 喺 `install-dir`，喺任何嘢被打包之前。佢會讀取啟動器同埋 `BambuStudio.dll` 嘅匯入同埋延遲匯入表，跟蹤每一個負載 DLL 佢哋拉入嘅，當一個匯入嘅 DLL 唔喺負載度都唔係 Windows 系統 DLL 嗰陣，工作流就會失敗。Visual C++ 執行階段計作負載，唔係作為 Windows。喺任何未打包嘅負載上本地執行：`py -3 scripts/ci/check_payload_imports.py <payload-dir>`；喺 `md3-v143` 負載上佢列出 24 個缺失 DLL。

啟動器亦會喺 `%TEMP%\bbs-launcher-trace.log` 記錄佢嘅早期決定(OpenGL 檢查、Mesa 選擇、`BambuStudio.dll` 載入結果)，呢個係喺啟動失敗得於記錄系統開始之前時，可以讀取資訊嘅地方。

## CycloneDX 負載清單

`scripts/ci/New-WindowsCycloneDxSbom.ps1` 發出 `BambuStudioMD3.cdx.json` 作為 CycloneDX 1.6。每一個安裝嘅檔案都會被表示為一個 `file` 組件，帶有相對路徑、位元組計數同埋小寫 SHA-256 摘要。文件將其頂級應用組件綁定到 Bambu Studio 版本、儲存庫同埋 40 字符嘅原始碼提交。

生成器會拒絕一個空負載、負載同埋輸出路徑重疊、源重析點、重複嘅組件名稱、缺失嘅 `bambu-studio.exe` 或格式錯誤嘅組件摘要。release 工作會重新驗證文件、要求至少 1,000 個組件、檢查版本同埋提交綁定嘅源 URL，並強制執行 GitHub 嘅 16 MiB SBOM 證明限制。

## Squirrel release 資産同埋證明

驗證完下載嘅構建資産後，release 工作會為 `Setup.exe` 建立構建來源同埋 SBOM 證明。一個候選 release 包含：

- `Setup.exe`；
- `Setup.exe.sha256`；
- `RELEASES`；
- 一個 `*-full.nupkg` 同埋任何生成嘅 `*-delta.nupkg` 檔案；
- `BambuStudioMD3.cdx.json`。

引導程序係刻意未簽署嘅，可能會觸發未知發佈者或 SmartScreen 警告。驗證下載完整性，使用：

```powershell
Get-FileHash .\Setup.exe -Algorithm SHA256
Get-Content .\Setup.exe.sha256
gh attestation verify Setup.exe --repo Ding-Ding-Projects/BambuStudio
```

校驗和同埋 GitHub 證明唔係 Authenticode 簽署，而唔會建立發佈者身份。冇簽署證書、私鑰、簽署服務或簽署認證被請求。

## 草稿到不可變發佈

release 工作會讀取儲存庫不可變 release 設定，若果佢冇被啟用就會失敗。佢會建立一個包含完整 Squirrel feed 嘅草稿、驗證目標、名稱、大小同埋 GitHub SHA-256 摘要與本地候選，解析最新狀態，並發佈驗證嘅草稿。啟用了不可變 releases 之後，生成嘅發佈標籤同埋資産喺發佈後唔能被改動。

若果響匹配嘅 release 仍然係草稿嘅時候出現錯誤，工作會刪除該草稿同埋佢嘅臨時標籤。若果狀態唔能被確定、目標不同或發佈可能已經完成，清理會安全地失敗，保留該 release 供檢查。一次重試只會移除同一提交嘅剩餘草稿，並驗證同埋重用一個同一提交嘅不可變發佈；佢從唔會改動一個已發佈嘅不可變 release。

## 驗證狀態

發佈同埋隔離嘅 Squirrel 安裝後，工作流會嘗試一個可選嘅隱藏桌面 GUI 捕捉。`scripts/md3/Capture-HostedReleaseGui.ps1` 讀取成功嘅安裝收據、檢查 release 標籤同埋原始碼提交，同埋重新雜湊已安裝嘅可執行檔案對照已安裝檔案同埋完整軟件包雜湊。佢會將版本釘選嘅無頭捕捉工具引導到執行器本地 Python 環境、建立一個新嘅應用數據目錄、並嘗試十一個工作區同埋偏好設定表面。一個任務所有嘅公開 RSA 密鑰會用一個新鮮嘅 AES-256-GCM 密鑰同埋每次執行嘅 nonce 加密一個 ZIP 嘅原始 PNG；RSA-OAEP-SHA256 會包裝 AES 密鑰。加密將執行 ID、原始碼提交、release 標籤、已安裝可執行檔案雜湊同埋映像雜湊作為認證數據綁定。私有 RSA 密鑰保持 DPAPI 保護喺 release 操作員嘅本地應用數據、此儲存庫之外。
工作流只會上傳加密嘅 ZIP、佢嘅小封套同埋 `receipt.json` 作為一個 30 天嘅執行資産。一旦一個支援嘅託管捕捉用一個新嘅輸出目錄開始，佢嘅收據會記錄稍後嘅預檢查同埋捕捉失敗。不支援嘅主機同埋現有輸出目錄會被拒絕，之前任何檔案都未被改動。原始映像保留喺可棄嘅執行器；佢哋從唔會以純文字形式上傳或發佈。
此步驟使用 `continue-on-error`，所以捕捉可用性唔係一個 release 門。

> [!IMPORTANT]
> AES-GCM 檢查加密嘅束符合佢嘅提供嘅認證元數據。
> 任何人有公開密鑰可以加密一個唔同嘅束，所以加密單獨唔認證佢嘅 GitHub 來源。喺本地解密或升級前，操作員必須使用
> `gh run view` 來比較確切嘅執行 ID 同埋原始碼提交，然後獨立驗證已發佈 release 目標同埋資産雜湊對照下載嘅 release 同埋安裝
> 收據。一個匹配嘅自我報告信封係唔夠嘅。

收據會記錄原始碼提交、release 標籤、安裝器同埋可執行檔案雜湊、捕捉方法、渲染幀雜湊、像素指標同埋狀態。佢成功嘅狀態係
`encrypted_capture_pending_restricted_review`。佢係來源同埋自動像素證據，
唔係已審查嘅 GUI 行為。操作員會執行 `scripts/md3/Open-HostedReleaseGuiEvidence.ps1`
用確切嘅執行 ID、原始碼提交、release 標籤同埋已安裝可執行檔案雜湊。該幫手
使用本地 DPAPI 密鑰、檢查認證綁定、強制執行一個固定嘅十一名映像允許清單同埋 ZIP 大小限制、並驗證每個映像雜湊，提取前。操作員
只會喺佢哋被寫入同埋重新雜湊喺一個唯一兄弟姐妹
目錄，原子地重命名到位之後才接收所有十一個檔案。現有輸出從唔會被覆蓋。操作員必須檢查解密嘅像素以獲得視覺質量同埋私人內容
喺任何映像被保留、嵌入或發佈之前。新鮮嘅可棄嘅設定檔唔會匯入用戶嘅本地安裝或數據。一個缺失或失敗嘅捕捉必須唔能被描述為已驗證嘅 GUI 行為。

喺接受一個候選之前，執行本地 release 合同同埋一鍵檢查、構建真實嘅 Squirrel 輸出、檢查構建資産嘅 README 捕捉矩陣、並記錄確切嘅提交、Actions 執行、release 標籤、安裝器 SHA-256、Squirrel 軟件包名稱、SBOM 組件計數、證明驗證、不可變狀態同埋已審查嘅截圖集。一個待決、已取消或缺失嘅遠端結果唔係 release 證明。

## 僅驗證嘅託管工作流

`.github/workflows/verify-release-evidence.yml` 係一個獨立調度嘅驗證工作流。佢從唔會編譯應用、建立 release、改動標籤或封閉發佈工作流。提供現有嘅不可變 `release_tag`、佢嘅確切 `expected_source_commit` 同埋一個 `verification_scope`。用 `diagnostic` 開始來檢查一個英文、淺色、100%、1200x800 元組。用 `behavior` 針對六個語言同埋主題託管工作，每一個記錄四個請求嘅尺度喺兩個視口大小。驗證檢查的提交係從不可變 release 原始碼提交獨立記錄嘅。
呢個工作流冇推送觸發器，只會由手動調度執行，通常喺 `main` 開始。2026 年 10 月 6 日之前，狹義嘅 `codex/hosted-behavior-verifier` 推送觸發器會喺驗證器
喺佢自己嘅分支審查期間，針對 `md3-v125` 同埋 `c5df6199e1a83b1c94be12e999c0b322fded8730` 執行一個固定診斷；嗰個觸發器已經移除。

每個工作使用 `Verify-HostedSquirrelInstall.ps1` 喺一個新鮮嘅託管 Windows 執行器上安裝同埋驗證已發佈嘅 Squirrel 軟件包。佢然後安裝版本釘選嘅無頭工具同埋 Pillow 喺工作本地 Python、呼叫 `drive-packaged-behavior.py` 對照已安裝嘅可執行檔案、同埋加密驅動器自己嘅報告、映像同埋可歸屬受限日誌。獨立嘅發佈工作流保留佢固定嘅十一表面架構 1 捕捉。架構 2 診斷唔會喺行為驅動器後啟動該舊捕捉路由，所以一個失敗嘅應用啟動仍然會保留佢嘅報告同埋受限日誌，若果存在。只有一個行為元組包括完整工作流驅動；其他元組檢查本地化佈局。一個診斷結果從唔會聲稱矩陣通過。
工作流使用一個有界超時、兩個併發矩陣工作、冇現有執行嘅取消、同埋一個安全失敗上傳。原始截圖、私人檔案同埋未審查嘅行為報告從唔會以純文字形式附加。

版本 2 加密信封綁定託管執行、release 標籤、release 原始碼提交、驗證提交、已安裝可執行檔案雜湊同埋捕捉映像、行為報告、行為映像同埋驅動器所有受限應用日誌嘅確切清單。清單為每個元組命名，並記錄每個位元組長度同埋 SHA-256。`Open-HostedReleaseGuiEvidence.ps1` 保留架構 1 讀支援同埋要求一個顯式預期驗證提交用於架構 2。佢拒絕重複名稱、遍歷、額外或缺失項目、無效計數同埋雜湊不匹配，提取前，傳入檔案到一個新嘅本地目錄。一個部分捕捉可以攜帶加密診斷，但佢嘅收據保持明確部分，且唔能被接受作為已驗證嘅 GUI 證據。缺失打印機、照相機或提供者訪問與失敗嘅探針、啟動或拆卸分開報告。操作員必須檢查解密嘅像素同埋報告用於隱私，保留或發佈之前。
Windows 錯誤報告轉儲係以明確嘅收據原因省略嘅，因為一個全球應用名稱匹配唔證明特定託管執行嘅 PID、進程建立間隔同埋已安裝可執行檔案身份。一個失敗嘅固定表面捕捉仍然會加密可歸屬行為報告同埋受限日誌，若果該等檔案被產生；佢保留一個部分裁決。
架構 2 使用獨立診斷、行為待審查同埋部分狀態。八個元組報告用於一個完整行為工作；一個報告用於一個診斷工作。
託管清單夾具檢查刻意提供遍歷同埋重複路徑，並要求兩者被拒絕，私鑰訪問或提取前。
佢也加密一個合成失敗診斷，冇映像或受限日誌、驗證部分收據同埋確切報告清單、同埋檢查元數據同埋認證綁定
到達擁有者密鑰邊界，而唔係喺託管執行器上提取純文字。

## 擁有者密鑰初始化同埋公開密鑰版本

原始 `hosted-gui-public.pem` 保持記錄為 `hosted-gui-public-v1.pem` 用於歷史信封。佢嘅匹配本地 DPAPI 槽，若果佢存在，係遺留嘅
`BambuStudio\HostedGuiEvidence\private-key.dpapi` 檔案，喺當前用戶嘅本地應用數據下。一個缺失嘅遺留私密鑰唔能從公開 PEM 或一個加密束重建。
該等歷史束保持不可讀，除非原始受保護鑰由佢嘅擁有者恢復。唔好替換一個新密鑰，聲稱舊證據被審查。

擁有者審查後，執行 `scripts/md3/Initialize-HostedGuiEvidenceKey.ps1 -Initialize` 本地喺帳戶下
會審查證據。腳本會建立一個新鮮 RSA 密鑰、用 DPAPI CurrentUser 保護佢嘅私有 PKCS#8 位元組喺一個獨立槽命名由公開 SPKI SHA-256、同埋只寫 `hosted-gui-public-v2.pem` 到儲存庫檢查。佢拒絕一個現有公開檔案或受保護槽，同埋唔能喺 Actions 執行。從唔好提交、上傳、登錄或披露受保護密鑰檔案或佢嘅未保護位元組。只審查同埋提交公開 PEM。版本 2 捕捉路由失敗封閉，直到該公開 PEM 喺驗證器檢查中出現。

新架構 2 信封記錄 `key_id` 同埋 `public_key_sha256`，兩者係同一公開 SPKI SHA-256。
開啟器選擇只嘅確切存檔版本 1 或贊同版本 2 公開密鑰，透過該 ID，
然後定位佢嘅對應本地受保護槽。未知或不匹配嘅 ID 係拒絕，私鑰訪問前；託管夾具涵蓋兩個情況。架構 1 讀取器同埋佢嘅遺留槽保持支援。一個成功嘅信封驗證仍然要求擁有者解密同埋隱私審查
佢任何報告或映像升級前。
每個新生成信封使用贊同版本 2 公開收件人，包括架構 1
從獨立 release 發佈工作流捕捉。架構編號描述證據清單格式，唔係加密密鑰版本。歷史信封仍然選擇存檔版本 1 公開密鑰同埋佢嘅遺留本地槽，透過佢哋嘅記錄指紋。
