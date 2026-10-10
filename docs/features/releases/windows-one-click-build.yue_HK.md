---
translation-of: windows-one-click-build.md
source-sha256: 907b18e1b0014e8d17eadda259623215e855ba74b220d91c8696c9e2cee2f685
review-status: agent-drafted
---

> 英文原文：[One-click Windows build and installer](windows-one-click-build.md)

# 一鍵 Windows 構建和安裝程式

`OneClickBuildInstaller.cmd` 係支援嘅本地項目入口，用於編譯 Release 有效負載並使用 Squirrel.Windows 打包。喺檔案資源管理器中雙擊佢或從終端執行佢：

```powershell
.\OneClickBuildInstaller.cmd
```

啟動器委託給 `scripts/windows/Invoke-OneClickBuild.ps1`，寫入成績單到 `artifacts/windows/one-click-build.log`，並喺以互動方式啟動時喺結束時暫停。構建輸出寫入到 `artifacts/windows/`：

- `squirrel/Setup.exe`：無簽署嘅 Squirrel 引導程式；
- `squirrel/RELEASES`：更新源索引；
- `squirrel/BambuStudioMD3-<version>-full.nupkg` 和任何已生成嘅差異套件。`<version>` 係 `<major>.<minor>.<patch*1000+N>`，例如產品 2.8.2、`N` = 106 就係 `2.8.2106`（Squirrel.Windows 只接受三個數字部分，而且將預發佈標籤當字串比較），所以就算 `version.inc` 冇變，套件都排得啱次序。一鍵構建會由 `-ReleaseNumber` 解析 `N`，然後 `BAMBU_RELEASE_NUMBER`，然後由 `gh` 見到嘅最新 `md3-v<N>` 發佈加一；三樣都冇嘅話，套件版本就淨係用產品版本（`2.8.2-build61`），日誌會講明。雲端發佈構建就改用 Windows 構建與發佈工作流程嘅執行編號：喺發佈工作分配標籤之前讀到嘅發佈編號，構建排隊嗰陣會重複（`md3-v155` 同 `md3-v156` 都係 `2.8.4155`），而執行編號只會跟推送次序增加；
- `squirrel/Setup.exe.sha256`：引導程式嘅 SHA-256 附屬文件；
- `BambuStudioMD3.cdx.json`：綁定到源提交嘅 CycloneDX SBOM。

## 佢做乜嘢

工作流檢查至少 40 GB 嘅可用空間並安裝缺失嘅普通先決條件：Git、Visual Studio 2022 或 2026 C++ 構建工具、完整 Windows SDK、CMake、Strawberry Perl 和 7-Zip。Strawberry Perl 當原生 `pkgconfiglite` 可執行檔不存在時供應 Windows `pkg-config.bat` 後援；構建明確匯出該包裝器，因此 CMake 唔會誤將 Strawberry 嘅無擴展幫手腳本誤以為可執行檔。現有支援嘅安裝會被重複使用。其他工具安裝無聲使用 `winget` 並接受套件／源同意；共用工具鏈幫手保留其發行商和引腳哈希檢查以供應商後援。依賴超級構建供應產品嘅哈希引腳 Node.js 和 pnpm 版本，因此工作流唔會替換無關係嘅系統 Node 安裝。

Squirrel.Windows 2.0.1 僅喺佢已喺用戶 NuGet 快取中可用時獲取。套件係從 NuGet 下載、根據提交嘅 SHA-256 引腳檢查、提取到用戶本地快取，然後由 `scripts/windows/Invoke-SquirrelPackage.ps1` 使用。唔接受無簽署指令、認證或簽署認證。

腳本暫存已驗證嘅 qpdf SDK 同 PDF 執行階段、編譯依賴項、編譯 Release 應用程式、暫存 CMake 安裝有效負載、下載並驗證 CI 使用嘅相同哈希引腳 Mesa llvmpipe 後援、建立 CycloneDX SBOM、使用確切源提交和存儲庫後設資料建立 Squirrel NuGet 套件、執行 `Squirrel.exe --releasify`、驗證 `Setup.exe`、`RELEASES`、完整套件和 Setup.exe 上嘅空 PE 安全目錄（無簽署），然後寫入校驗和附屬文件。

預設值係增量嘅。當快取可能過時時使用清潔重新構建：

```powershell
.\OneClickBuildInstaller.cmd -BuildMode Clean
```

引導或檢查而唔編譯：

```powershell
.\OneClickBuildInstaller.cmd -BootstrapOnly
.\OneClickBuildInstaller.cmd -Plan
```

安裝程式係無簽署嘅。佢唔自動啟動。要喺成功打包後執行佢，使該狀態變更選擇明確：

```powershell
.\OneClickBuildInstaller.cmd -Install
```

自動化可以喺呼叫 CMD 啟動器前設定 `BAMBU_ONE_CLICK_NO_PAUSE=1`。一次僅可執行一個副本；跨進程互斥鎖拒絕第二次啟動，在佢可以寫入共用構建快取前。

## 已驗證嘅 qpdf SDK 同 PDF 執行階段

原生轉換器係對住 qpdf C API 編譯嘅；除非 `LOCAL_CONVERTER_QPDF_SDK` 指向已驗證嘅 qpdf 12.4.2 SDK，否則設定 `src/slic3r` 會出錯停低。一鍵構建會自動暫存佢，而且喺依賴構建之前做，所以下載或者雜湊出錯幾分鐘內就會停，唔使等幾個鐘：

- `scripts/windows/Install-LocalPdfTools.ps1` 由 `scripts/windows/local-pdf-tools.json` 攞封存檔名、大小同 SHA-256。有權杖或者已登入嘅 GitHub CLI 時，佢用 `gh release download` 下載官方 `qpdf-12.4.2-msvc64.zip` 發佈資產；冇嘅話，或者嗰次下載失敗，就經 HTTPS 由同一個發佈下載。無論邊個副本，都要先符合釘住嘅大小同 SHA-256 先會解壓任何嘢。已驗證嘅封存檔會留喺 `artifacts/local-pdf-cache`。
- SDK（標頭同 `qpdf.lib` 匯入程式庫）暫存喺 `artifacts/local-pdf/sdk`，並以 `-DLOCAL_CONVERTER_QPDF_SDK:PATH=...` 傳俾設定。SDK 路徑係設定重用檢查嘅一部分，所以喺呢個要求之前設定嘅構建樹會重新設定，唔會直接重用。
- 執行階段（`qpdf30.dll`、`qpdf.exe`、官方發行版附帶嘅 Microsoft 執行階段 DLL、授權聲明同清單）暫存喺 `install-dir/tools/pdf`，即係打包咗嘅轉換器工作程式載入佢嘅位置，所以會同有效負載其他部分一齊入 SBOM 同 Squirrel 套件。
- 安裝腳本需要 PowerShell 7，而一鍵產生器係喺 Windows PowerShell 5.1 行。冇 `pwsh.exe` 嘅時候，引導會安裝 `Microsoft.PowerShell` winget 套件，產生器只喺呢一步啟動 `pwsh.exe`。
- 已經驗證過嘅樹會直接重用，唔會再下載。同釘住值唔一致嘅樹，或者失敗嘗試留喺 `install-dir/tools` 嘅 `pdf.stage-*` 目錄，會令構建停低而唔會被改動；檢查同移除佢之後再執行。

如果要為手動 CMake 設定暫存同一批檔案，喺儲存庫根目錄執行：

```powershell
pwsh -NoProfile -File scripts/windows/Install-LocalPdfTools.ps1 `
  -Destination artifacts/local-pdf/runtime -SdkDestination artifacts/local-pdf/sdk
cmake -S . -B build -DLOCAL_CONVERTER_QPDF_SDK:PATH="$PWD/artifacts/local-pdf/sdk" <other options>
```

將已驗證嘅執行階段複製到已安裝轉換器工作程式旁邊嘅 `tools/pdf`，嗰個構建先可以用 PDF 操作。雲端 Windows 構建工作流程同 `build_win.bat` 都用同一個方法暫存 SDK；詳見 [Bundled PDF engine](../converter/pdf-engine.md)。

## 故障模式和恢復

- 依賴項安裝可能需要 Windows 提升或重新啟動。喺批准應商安裝程式或重新啟動後重新執行相同指令；已完成嘅先決條件被偵測和重複使用。
- 清潔構建可能需要超過 40 GB 和數小時。成績單標識確切失敗嘅階段和退出碼。
- 網絡存取需要缺失套件、引腳 Mesa 檔案、未快取嘅引腳 qpdf 封存檔，同埋當佢唔快取時嘅哈希引腳 Squirrel.Windows NuGet 套件。
- 跟蹤嘅工作樹編輯可以本地編譯，但 Squirrel nuspec 僅可以記錄目前 Git 提交。當呢個使本地有效負載不可重現時工作流警告。
- 生成嘅 Squirrel 套件必須包含 `lib/net45/bambu-studio.exe`；缺少可執行檔、缺少 `RELEASES`、不匹配嘅校驗和或 Setup.exe 上嘅非空 PE 安全目錄失敗關閉。

沒有圖案、源內容或構建日誌被傳輸，除了聲明嘅套件、Git 和構建所需嘅引腳成品端點外。

Visual Studio 會略過缺少必要檔案或預設 x64 編譯器嘅舊註冊，產品名稱同路徑取自同一個可用實例。搵唔到可用實例或缺少 SDK 時，Microsoft 簽署嘅 VS 2026 Stable 引導程式會明確使用 `%LOCALAPPDATA%\BambuStudioMD3\toolchain\BuildTools2026` 同 `--norestart`。唔會移除或修復其他產品嘅註冊，亦唔會繞過所需嘅管理員批准。

實例選擇依照 [Microsoft 命令列安裝參數](https://learn.microsoft.com/en-us/visualstudio/install/use-command-line-parameters-to-install-visual-studio)。回歸測試涵蓋失效同可用註冊、產品與路徑一致、預設編譯器缺失，以及有引號嘅明確安裝目標，測試唔會啟動安裝程式。此儲存庫冇追蹤 Git LFS 物件，支援嘅腳本唔會安裝或呼叫 Git LFS。
Microsoft 要求並存安裝嘅主版本、產品版本同更新通道組合必須唯一。同一組合嘅既有註冊仍可能阻止明確目標；應記錄安裝程式錯誤，唔好刪除或改寫其他產品嘅註冊。模擬測試通過唔代表主機安裝或原生構建成功。

可用嘅 VS 2022 安裝繼續支援 CMake 3.21。VS 2026 需要 CMake 4.2 或更新但低於 5.0，先有 `Visual Studio 18 2026` 產生器。依賴同應用程式設定都用 `CMAKE_GENERATOR_INSTANCE` 鎖定所選路徑。源碼構建會向 `build_win.bat` 傳入所選主版本、產品同 `BAMBU_VS_INSTALLATION_PATH`，展開路徑之前會驗證註冊、必要檔案、支援版本同不安全 CMD 字元。冇設定覆寫時保留原有偵測。VS 2026 Stable 同未完成嘅 VS 2022 Release 註冊屬於不同主版本／通道；原生構建相容性仍要以真實構建結果證明。

VS 2026 亦會使用同一註冊記錄嘅四段數字 `installationVersion`，按 CMake 文件嘅 `location,version=` 語法傳入。批次入口會核對 `BAMBU_VS_INSTALLATION_VERSION` 同所選記錄完全相符。偵測包括預發佈同未完成註冊，再獨立檢查必要編譯器檔案。日誌會記錄路徑、版本、預發佈狀態同註冊完整性；註冊未完成唔代表檔案不存在，檔案存在亦唔代表 MSBuild 可以執行。

可選執行 `scripts/ci/Test-BuildFromSourceHelpers.ps1 -ProbeHostCMake`，用所選實例附帶嘅 CMake 做有界限、只設定唔編譯嘅測試。即使直接編譯器測試通過，MSBuild 組件載入錯誤仍然係工具鏈阻塞。安裝暱稱使用 `BambuMD3`，符合 Microsoft 最多十個字元嘅限制。

偵測亦會以 `-nologo -version` 啟動確切嘅 x64 MSBuild，限時十五秒。啟動錯誤、逾時或冇數字版本會排除該實例、記錄原因，再試下一個。全部都唔可用先執行支援嘅 Stable 安裝路徑，唔會複製組件入其他安裝，亦唔會修復其他產品嘅安裝。

## 全新 Windows 引導同管理員交接

`build.bat /s` 同 `build-installer.bat /s` 都會執行共用嘅 PowerShell 產生器，並傳回佢嘅退出碼。共用嘅根目錄入口啟動器而家會喺攞構建互斥鎖、開成績單或者安裝先決條件之前，經正常嘅 Windows `RunAs` 同意提示要求管理員批准。提升咗嘅幫手主控台會收埋；原生 UAC 同意提示仍然係互動嘅。已經提升咗嘅呼叫會直接繼續。取消咗或者冇得提升就會傳回 `1223`。交接會保留現有嘅進程範圍執行原則，唔會改持久原則。完整嘅原始參數陣列會當資料序列化，提升咗嘅主機會用呢個陣列呼叫原本嗰個 `build.bat`、`build-installer.bat` 或 `OneClickBuildInstaller.cmd`；`-Plan` 仍然係唯讀，唔會要求提升。靜默模式只係收埋啟動器嘅訊息，唔會跳過管理員同意。

現有嘅引導會喺缺少嘅時候安裝 Git、Visual Studio 2026 C++ 構建工具同佢嘅 Windows SDK、CMake、Node、Strawberry Perl、Python 3 同 7-Zip。如果冇 WinGet，而家會用 Microsoft 嘅 `Microsoft.WinGet.Client` 引導同 `Repair-WinGetPackageManager -AllUsers`，然後重新整理目前進程嘅路徑。呢個做法唔會改持久執行原則、套件儲存庫信任、防毒設定或者主機電源狀態。兩個入口另外都會安裝自動化配套程式需要嘅穩定版 .NET 10 SDK，配套程式會喺只構建路線返回之前暫存好。

Microsoft 喺 [Windows Package Manager installation](https://learn.microsoft.com/en-us/windows/package-manager/winget/) 記載咗 WinGet 引導。網絡、發行商、套件服務、管理員同意同機器註冊嘅阻塞都會照報，唔會當成安裝成功。原生工具鏈安裝程式保留原有嘅 `--norestart` 行為。兩個入口都唔會重新啟動或者關閉主機。

打包安裝程式嘅時候，如果冇得做已驗證嘅發佈探索，就要提供 `-PreviousPackageVersion`（或者 `BAMBU_PREVIOUS_PACKAGE_VERSION`）。引導唔會自己作發佈歷史，亦唔會安裝憑證。有效嘅依賴快取可以重用，但係受影響而又過時嘅原生輸入，仍然要做記載咗嘅源路徑產生器修復同真正重新構建。兩個入口仍然係正式生產指令；呢個源碼修復未喺全新 Windows 安裝上面執行過，亦唔聲稱做過全新安裝驗證。

原本嘅啟動器會等提升咗嘅產生器完成，並傳回佢真正嘅進程退出碼。產生器 `exit 1` 會直接結束嗰個子主機；幫手嘅成功／失敗檢查只會喺產生器正常返回時適用。啟動異常會另外傳回 `1223`。以上係源碼層面嘅控制流程事實；呢條實作線冇實際試過取消、子進程失敗同全新主機執行。

提升交接喺 `scripts/windows/Invoke-BuildEntryPoint.ps1`。佢只接受儲存庫入面三個確切嘅啟動器，將入口身份同完整原始參數陣列序列化做 JSON 資料，再經 PowerShell 原生呼叫用參數陣列重新啟動同一個批次檔。佢唔會砌原始嘅 `cmd /c` 指令字串。提升後重新進入會呼叫產生器一次。根目錄 `build.bat` 保留只構建嘅行為，兩個安裝程式啟動器就保留打包行為。`/s` 同 `--silent` 會經過提升保留落嚟，只喺產生器邊界先過濾。直接呼叫產生器係內部路線，唔能夠證明任何一個根目錄批次入口執行過。

喺腳本邊界，啟動器會驗證支援嘅具名參數，並用雜湊表綁定，而唔係用按位置嘅陣列展開。佢接受值參數 `BuildMode`、`OutputDirectory`、`DependencyCacheDirectory`、`ReleaseNumber` 同 `PreviousPackageVersion`，加上開關 `Install`、`BootstrapOnly`、`Plan` 同 `BuildOnly`。唔認識或者重複嘅名會被拒絕。根目錄 `build.bat` 會強制 `BuildOnly=true`，就算呼叫者明確俾咗 false 都一樣。提升期間，原本嘅原生參數陣列仍然會重新進入同一個批次檔；腳本綁定喺呢個來源邊界之後先發生。

提升啟動器只會讀現有嘅 `PSExecutionPolicyPreference` 進程環境值，按認得嘅執行原則值驗證，再保留俾子進程。冇呢個值或者係 `Undefined` 就唔加呢個參數。唔認識嘅值會停止交接。佢唔會載入可選嘅 `Microsoft.PowerShell.Security` 模組，亦唔會改機器／使用者原則；批次入口嘅確切來源保持不變。

CMake 引導會先保留 PATH 入面已經符合支援最低版本、又低過不含在內最高版本嘅可執行檔。否則佢會用同樣嘅版本範圍檢查所選可用 Visual Studio 實例嘅 `Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe`，將嗰個目錄加到產生器進程 PATH 最前面，並喺重新整理 PATH 時保留。兩個候選都唔合格嘅話，就照舊用套件管理器／供應商安裝做後援。偵測喺提升之後先做，所以唔使靠淨係呼叫者改過嘅 PATH 撐過 `RunAs`。源碼缺口已經確認；同一時間有個 WinGet 操作喺度行，佢嘅目的未查明，亦唔歸咎於呢個缺口。呢個修復仲未有執行證明。

## 可搬遷嘅 OpenCV 資料查找

打包用嘅 OpenCV 產生器會啟用 `OPENCV_RELOCATABLE_DATA_LOOKUP`。佢嘅源碼修補會略去絕對路徑嘅 `OPENCV_INSTALL_PREFIX` 巨集，並輸出空白嘅 `OPENCV_BUILD_DIR`；當呢個根目錄係空白嘅時候，執行階段程式碼會明確略過兩個構建目錄搜尋分支。咁樣可搬遷嘅已安裝套件就唔會用開發樹後援，而唔係用虛構嘅邏輯路徑取代執行階段嘅檔案系統位置。

明確嘅資料設定同搜尋路徑覆寫保持不變。現有嘅 `OPENCV_INSTALL_DATA_DIR_RELATIVE` 查找仍然會由執行緊嘅模組推算已安裝資料位置，包括設定咗嘅相對 `etc` 關係。必需／搵唔到嘅行為、由實際目前目錄做嘅源碼樹探索、檔案系統檢查同診斷都仍然可用。產生器選項停用嘅時候，普通 OpenCV 構建保留原有嘅常數。

喺自己擁有嘅 OpenCV 源碼目錄重新套用第四個源碼修補，啟用選項重新設定子構建，重建佢生成嘅資料標頭／靜態程式庫，然後經確切嘅根目錄批次入口重新連結同重新暫存應用程式。現有嘅 `version_string.inc` 對應係另一回事，保持原狀。呢條修復線冇執行測試、構建、執行階段互動或者截圖；最終有效負載仍然要檢查。

提升啟動器攞到確切嘅提升入口主機嘅進程控制代碼之後，會用 `Process.WaitForExit` 等佢完成，並喺釋放控制代碼之前讀退出碼。佢唔用 `Start-Process -Wait`，因為嗰個會等埋所有後代工作，包括產生器完成之後仲閒置緊、可重用嘅 MSBuild 伺服器。呢個改動唔會停止或者終止任何後代進程，原本嘅批次參數、隱藏主機同正常 UAC 同意都保持不變。進程完成仍然同打包／發佈驗證係兩回事。

## 可搬遷嘅 wxWidgets 安裝前綴

依賴產生器會啟用 `wxBUILD_RELOCATABLE_INSTALL_PREFIX`，並對 wxWidgets 嘅 CMake setup 產生器同 `wxGetInstallPrefix` 套用源碼修補。Windows 套件生成嘅 setup 標頭會略去構建機器嘅 `wxINSTALL_PREFIX`，並啟用可搬遷嘅執行階段後援。`WXPREFIX` 仍然係第一個明確覆寫。否則，後援會由 `wxStandardPaths` 攞實際可執行檔路徑，傳回佢所在嘅目錄；`wxGetDataDir` 保留原有嘅 `share/wx` 後綴。咁做用嘅係真實嘅執行階段位置，而唔係捏造出嚟嘅邏輯檔案系統前綴。其他平台或者停用咗呢個選項嘅構建，就保留上游編譯時前綴嘅行為。

喺自己擁有嘅 wxWidgets 源碼重新套用修補，重新設定佢嘅 setup 標頭，重建受影響嘅 base 物件／靜態程式庫同依賴佢嘅應用程式輸出，然後執行確切嘅根目錄批次生產指令，再檢查最終有效負載。呢個修復唔會改已編譯嘅位元組，亦唔會放寬發佈檢查。呢條源碼線冇執行測試、構建、執行階段檢查或者截圖。

wxWidgets 依賴釘住 `bambulab/wxWidgets` 嘅 `a9d946902685b9946d8775f07d2a73a9b5bef394`，即係用嚟寫可搬遷前綴修補嘅乾淨快取源碼。上游提交 API 亦獨立傳回同一個確切 SHA。產生器唔再跟住會變嘅 `master`，所以全新嘅依賴簽出會攞到同修補基準一樣嘅源碼版本。

## 編譯工具同安裝目錄分開

Visual Studio 引導程式同 PDF 構建預設使用 %LOCALAPPDATA%\BambuBuildTools\VS2026，放喺 Squirrel 嘅 %LOCALAPPDATA%\BambuStudioMD3 安裝目錄之外。完整安裝會替換成個產品目錄；之前編譯工具放咗喺入面，Setup 刪除受保護嘅工具檔案時就失敗，未開始安裝應用程式。

回歸檢查會讀實際引導參數，要求編譯工具唔可以放喺 Squirrel 目錄入面，亦檢查 PDF 構建預設位置。已有註冊工具可以繼續用嚟構建，引導程式唔會改名或者刪除佢。受影響嘅電腦需要透過 Visual Studio 正式解除安裝及重新安裝，必要時由用戶同意管理員權限；改源碼唔會自動搬走舊工具。
