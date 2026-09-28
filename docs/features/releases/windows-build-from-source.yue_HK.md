---
translation-of: windows-build-from-source.md
source-sha256: 8c25c0d2f3afbc1e0636197c6c9bbe629444075270768afcc642c01cb9029596
review-status: agent-drafted
---

> 英文原文：[Build from source (Windows installer)](windows-build-from-source.md)

# 從源碼構建（Windows 安裝程式）

> **歷史備註：** 目前嘅發佈路徑係 [Native Windows installer](windows-native-installer.md) 度記載嘅無簽名 Squirrel.Windows 軟件包。下面描述嘅互動式 NSIS 從源碼構建助手只係保留用來審計同歷史記錄，現時嘅構建同發佈工作流程唔會呼叫佢。

## 概述

Windows 安裝程式提供咗一個可選嘅、互動式 **從源碼構建** 安裝來源，同預設嘅預製構建並列。當用戶選擇呢個選項時，安裝程式會啟動開發者工具鏈、克隆與產生安裝程式嘅同一個倉庫同精確嘅 40 字元提交、喺用戶嘅機器上編譯應用程式、暫存構建好嘅負載、然後將**同一個**所有權、恢復同卸載流程應用到呢個負載。兩個來源之間唯一嘅差異係負載係點樣產生嘅，同之後所有權檔案係點樣被列舉去卸載。

從源碼構建刻意限制為互動式安裝。佢喺 `/S` 底下永遠到達唔到，而且喺 CI 度永遠唔會執行（見「CI 劣化」）。佢係為進階用戶而設，佢哋想要喺本地構建而唔係信任預製嘅二進制檔案。

源碼選擇會記錄喺登錄檔中嘅 `InstallSource`（`prebuilt` | `from-source`）。卸載程式會根據呢個值進行分支；睇 [Native Windows installer](windows-native-installer.md) 基礎安裝程式功能文件，瞭解喺兩個來源之間共享嘅所有權同卸載語義。

## 組件

- `packaging/windows/BambuStudioMD3.nsi` ： 裝載安裝來源選擇器頁面、非關閉嘅構建進度頁面、非同步過程啟動、輪詢計時器、退出碼分支，以及源碼負載複製同清單驅動嘅卸載分支。
- `packaging/windows/build-from-source/Build-FromSource.ps1` ： 由構建進度頁面非同步執行嘅協調器。
- `packaging/windows/build-from-source/Toolchain.ps1` ： 點源工具鏈檢測/啟動。
- `packaging/windows/build-from-source/Opencode.ps1` ： 點源 opencode 安裝、項目本地允許設定排放器同有界修復呼叫。

呢三個 PowerShell 助手係用 `File` 編譯到安裝程式入面，喺執行時提取到一個私有插件目錄（`$PLUGINSDIR\bfs`），而且永遠唔會進入軟件負載，所以軟件物料清單同所有權清單都唔會受到佢哋嘅影響。

## 端到端流程

當用戶選擇「從源碼構建」時，構建進度頁面會：

1. 喺 `%LOCALAPPDATA%\codingmachineedge\BambuStudioMD3-FromSource\<yyyyMMdd-HHmmss>` 建立一個每次執行嘅**會話目錄**。佢係用戶可寫嘅，完全住喺固定安裝目錄之外，所以佢永遠唔會觸發所有權或重新解析守衛。
2. 將三個助手提取到 `$PLUGINSDIR\bfs`。
3. 通過 `kernel32::CreateProcessW` 以 `CREATE_NO_WINDOW` 非同步啟動 `Build-FromSource.ps1`，傳入 `-SessionDir`、`-CloneUrl`（`PRODUCT_SOURCE_REPO_URL`）、`-Tag`（`PRODUCT_SOURCE_TAG`，儘管名字係舊嘅，呢個必須係一個完整嘅提交）、`-LanguageMode` 同 `-PayloadOut`（`<session>\install-dir`）。佢會儲存該過程嘅句柄用於輪詢。如果 `CreateProcessW` 失敗，頁面會將其視為退出碼 40（致命）。
4. 鎖定視窗（見「非關閉視窗」）並啟動大約 500 毫秒嘅輪詢計時器。

`Build-FromSource.ps1` 然後喺會話目錄內執行呢個管道，點源兩個助手：

1. **前置檢查** ： 會話目錄可寫、會話驅動上至少有大約 40 GB 嘅空閒空間，同一個 TCP 到達測試到 `github.com:443`（8 秒超時）。任何失敗都會退出 **30**。
2. **工具鏈啟動** ： 如果缺少，安裝 Git、Node.js LTS、Visual Studio 2022 C++ 構建工具同 CMake（見「工具鏈啟動」）。失敗退出 **10**。
3. **克隆 + 檢出** ： `git clone <CloneUrl> <session>\src`、`git checkout --detach <commit>`，然後比較 `git rev-parse HEAD` 到請求嘅提交。分支、共享版本標籤、縮寫物件 ID 或不匹配嘅檢出會被拒絕；失敗退出 **11**。
4. **opencode 安裝 + 設定** ： 全局 `npm install -g opencode-ai`，然後將項目本地 `opencode.json` 寫到克隆度（見「opencode 啟動同修復循環」）。安裝失敗退出 **12**。
5. **用有界修復循環進行構建** ： 編譯依賴關係、編譯應用程式同暫存負載，使用 CMake 安裝目標，每個階段都由 opencode 修復循環包裝（見下面）。用盡修復預算退出 **20**。
6. **暫存驗證 + 清單** ： 確認 `<session>\install-dir\bambu-studio.exe` 存在（否則退出 **20**），然後寫入 `<session>\owned-manifest.txt`。成功退出 **0**。

構建步驟係記載嘅 Windows 構建路徑，釘住驗證嘅已安裝 Visual Studio 2022 軟件同一個配置：`build_win.bat -v 17 -p <detected-product> -c Release -d <session>\deps -s deps`，然後 `build_win.bat -v 17 -p <detected-product> -c Release -s app`，然後 `cmake --install build --config Release --prefix <session>\install-dir`。檢測同釘住現有嘅 Community、Professional、Enterprise 或 Build Tools 軟件可以避免選擇唔相關嘅更新版本 Visual Studio 安裝同安裝冗餘 SKU。安裝命令嘅 `--prefix` 覆蓋會喺構建無提升嘅來源時去掉預設嘅 `Program Files` 前綴，同時產生與 CI 相同嘅暫存佈局。

## 工具鏈啟動

用戶嘅同意係安裝來源選擇本身；個別工具安裝靜悄悄執行，冇每個工具嘅提示。對於每個工具，助手首先驗證可用版本同必要組件，然後只有當該驗證失敗時先安裝或升級，優先使用 `winget`，退回到一個釘住嘅官方供應商安裝程式：

| 工具 | 探測 | winget id | 供應商退回（靜悄悄） |
|---|---|---|---|
| Git | `git` 命令存在 | `Git.Git` | Git for Windows 2.54.0、`/VERYSILENT /NORESTART /NOCANCEL /SP-` |
| Node.js LTS | LTS 中繼資料、Node 主要版本 `22` 或更新版本、`x64` 架構同 `npm.cmd` 都存在 | `OpenJS.NodeJS.LTS` | Node.js 22.22.2 LTS MSI、`msiexec /i /qn /norestart` |
| VS 2022 C++ 工具鏈 | `vswhere` 17.x Community、Professional、Enterprise 或 Build Tools 軟件，包含 VC x64 工具集、`VsDevCmd.bat`、MSBuild 同一個完整嘅 Desktop Windows SDK >= `10.0.22000.0`（UM/shared/UCRT 標頭加 x64 UM/UCRT 庫）| `Microsoft.VisualStudio.2022.BuildTools` 帶著 VCTools 工作負載、Windows 11 SDK `10.0.26100` 同 VC CMake 組件（只有當冇合適嘅軟件存在時） | `https://aka.ms/vs/17/release/vs_BuildTools.exe` 帶著相同嘅 `--add` 集、`--quiet --wait --norestart` |
| CMake | 已解析版本 >= `3.21.0` 同 < `5.0.0`（支援嘅 4.x 行包括 4.4 退回） | `Kitware.CMake` | CMake 4.4.0 MSI、`/qn /norestart ADD_CMAKE_TO_PATH=System` |

winget 呼叫使用 `-e --silent --accept-package-agreements --accept-source-agreements`。啟動程式請求 SDK `10.0.26100`，但接受任何完整嘅 SDK 從 `10.0.22000.0` 開始，所以已安裝嘅更新版本 SDK 唔會觸發冗餘嘅 Visual Studio 修改。CMake 可能已經由 Visual Studio VC CMake 組件提供，但只有當其已解析版本滿足最小值時先跳過。喺任何下載嘅退回安裝程式執行之前，助手需要一個有效嘅 Authenticode 簽名，其發佈者身份完全符合該工具嘅允許清單：Johannes Schindelin for Git for Windows、OpenJS Foundation for Node.js、Kitware, Inc. for CMake 同 Microsoft Corporation for Visual Studio。釘住嘅 Git、Node 同 CMake 發佈亦必須符合佢哋嘅釘住 SHA-256 摘要。可變嘅 Visual Studio `aka.ms` 啟動程式冇穩定嘅摘要，所以佢係由 Authenticode 同其精確嘅 Microsoft 發佈者身份保護。

喺每個 winget 或供應商嘗試之後，登錄檔 `PATH` 條目被附加到現有過程條目。合併保留便攜式同呼叫者提供嘅優先權，同時去重複所有條目。任何仍然缺失、超出範圍或不完整嘅工具都會拋出異常，協調器會將其映射到退出 **10**。

## opencode 啟動同修復循環

opencode 係自動修復助手。佢係用 `npm install -g opencode-ai` 安裝嘅（需要喺前一個步驟啟動嘅 Node.js LTS）。喺第一次修復執行之前，同之後每次執行之前，`New-OpencodeAllowConfig` 會寫入 `<clone>\opencode.json`。

配置授予每個**行動**權限類別 `allow`，並保持兩個守衛喺 `deny`：

```json
{
  "$schema": "https://opencode.ai/config.json",
  "permission": {
    "edit": "allow",
    "bash": "allow",
    "webfetch": "allow",
    "question": "deny",
    "external_directory": "deny"
  }
}
```

因為檔案係項目本地，授予只適用於使用克隆目錄作為其工作目錄啟動嘅 opencode 會話。作為最佳努力，排放器亦會從 `https://opencode.ai/config.json`（15 秒超時）獲取實時 opencode 配置模式，並將其列出嘅任何其他行動類別設定為 `allow`，所以將來嘅行動類別被自動覆蓋；如果網絡唔可用，上面嘅基礎行立住。`question` 同 `external_directory` 喺任何模式傳遞後都被重新強制為 `deny`。

每個構建階段都由修復循環包裝。喺任何失敗嘅階段上，協調器會捕獲步驟名稱同最後大約 200 行日誌，撰寫一個修復提示，要求 opencode 通過編輯倉庫中嘅檔案來診斷同修復原因，然後停止（明確告訴佢唔要執行構建本身），然後喺克隆目錄中執行一個非互動式 opencode 會話。呼叫會設定工作目錄到克隆、從 `$null` 重新導向 stdin 以便阻止嘅提示永遠唔會等待輸入，同傳遞一個自動批准旗標（`--yes`）當已安裝嘅 opencode 版本暴露一個時。失敗嘅階段然後被重新執行。

修復預算係跨越整個構建嘅**累積 5 個修復-重建循環嘅上限**（嘗試計數器喺階段間唔會重置）。當一個階段喺第五個修復循環後仍然失敗時，構建退出 **20**。`attempt` 計數同 `maxAttempts` 5 被表面化喺進度 UI 中作為「修復 N/5」標題。

## 進度協議（無插件）

PowerShell 協調器與 NSIS 輪詢器通過會話目錄中嘅兩個檔案通訊，所以冇 NSIS JSON 插件係必要嘅：

- `build.log` ： 一個只追加嘅副本。佢係用一個 UTF-16LE BOM 一次播種同之後冇 BOM 嘅追加，所以 Unicode 安裝程式原生讀住。
- `status.json` ： 一個單一狀態物件原子重寫（寫到 `status.json.tmp`，然後 `Move-Item -Force`）。佢係 UTF-16LE 帶著 BOM 同格式化每行一個欄位，所以 NSIS 讀器可以喺沒有解析 JSON 嘅情況下匹配開頭嘅鍵記號：

```json
{ "schema":1, "state":"running|success|failed|fatal",
  "phase":"preflight|toolchain|clone|opencode-install|deps|app|install-target|repair|stage",
  "step":"<human label>", "attempt":0, "maxAttempts":5,
  "pctHint":0, "exitCode":null, "updatedUtc":"<iso8601>" }
```

NSIS 輪詢計時器大約每 500 毫秒觸發一次同：讀取 `status.json` 以設定步驟標籤同「修復 N/5」標題；讀取 `build.log` 嘅尾部（尋求到最後大約 2000 位元組，儲存喺一個偶數位元組偏移用於 UTF-16LE）到只讀日誌尾編輯並將其滾到底部；保持行進條動畫；同呼叫 `GetExitCodeProcess`。當該過程報告 `STILL_ACTIVE`（259）時，佢保持輪詢；一旦過程退出，佢停止計時器、讀取退出碼、關閉句柄並分支。

## 退出碼合同

協調器嘅過程退出碼係被 NSIS 消費嘅合同：

| 碼 | 意思 | 安裝程式行動 |
|---|---|---|
| 0 | 負載暫存同清單寫入 | 進行到構建負載嘅共享所有權安裝 |
| 10 | 工具鏈啟動失敗 | 致命錯誤頁面（碼 + 日誌路徑）、退出、冇寫入負載 |
| 11 | 克隆或檢出失敗 | 致命錯誤頁面 |
| 12 | opencode 安裝失敗 | 致命錯誤頁面 |
| 20 | 構建喺最大修復循環後失敗，或暫存負載唔完整 | 有界失敗對話框，提供預製退回或讓視窗可關閉 |
| 30 | 前置檢查（網絡 / 磁碟 / 可寫）失敗 | 致命錯誤頁面 |
| 40 | 意外協調器錯誤（陷阱），或過程無法啟動 | 致命錯誤頁面 |

喺成功時，頁面自動推進到共享所有權安裝：`File` 基礎提取被跳過，暫存負載用 `CopyFiles /SILENT` 從 `<session>\install-dir` 複製到固定安裝目錄，同 `owned-manifest.txt` 被複製作為 `.md3-owned-manifest.txt`（本身被擁有同喺卸載時最後被移除）。所有其他啟動步驟 ： 所有權標記、恢復卸載程式、登錄檔註冊，包括 `InstallSource=from-source`、重新解析守衛同 `bootstrap_cleanup` -> `ready` 順序 ： 都係共享、未改動流程。

喺有界失敗碼上，一個克制嘅雙語對話框報告日誌路徑同提供安裝預製版本作為替代。選擇退回會設定 `InstallMode=prebuilt` 同推進到正常預製安裝；拒絕會讓（現在可關閉）視窗讓用戶可以取消。喺任何致命碼上，一個克制嘅雙語對話框報告退出碼同日誌路徑，安裝程式設定非零錯誤等級，同佢退出冇將任何負載寫入固定安裝目錄。

## 非關閉視窗

當一個源碼構建執行時，構建進度頁面係安裝中唯一非關閉視窗，只係喺構建開始同構建終止之間。喺頁面嘅 `Show` 上，安裝程式隱藏同禁用取消、禁用後退/前進，同用 `DeleteMenu` 從視窗嘅系統選單移除 `SC_CLOSE`，呢個會灰出標題欄關閉按鈕同阻止 Alt+F4。自定義中止鈎子亦拒絕喺構建活動旗標設定時退出。

原因係源碼構建會安裝機器可見嘅開發者工具同執行一個長嘅、多階段編譯，帶著自動修復循環；喺中途關閉視窗會留下一個部分安裝嘅工具鏈同半完成嘅構建冇乾淨恢復。鎖定視窗會令執行從用戶嘅觀點原子直到達到一個已定義嘅終端狀態。

關閉喺恰好三個終端轉換重新啟用，每個恢復預設系統選單（`GetSystemMenu` 恢復）同喺展示下一個或終端頁面前重新啟用後退/前進/取消：

1. **成功（退出 0）** ： 啟用前進同推進到所有權安裝；視窗從完成頁面起係正常嘅。
2. **有界失敗（退出 20）** ： 重新啟用關閉同展示退回對話框（日誌路徑加預製而不係選項）。
3. **致命錯誤（10/11/12/30/40）** ： 重新啟用關閉同展示致命錯誤對話框（碼加日誌路徑）；冇負載被寫入。

## 源碼構建嘅卸載

因為源碼構建嘅檔案集可以從編譯預製清單漂移（例如不同嘅工具鏈 DLL），卸載程式分支喺登錄檔 `InstallSource` 值上：

- `prebuilt` ： 現有編譯嘅 `Delete`/`RMDir` 巨集、逐字未改動。
- `from-source` ： 擁有清單 `.md3-owned-manifest.txt` 驅動移除。每個 `F|` 檔案條目通過安全相對路徑檢查（拒絕絕對、驅動限定同 `..` 遍歷路徑）同與預製路徑相同嘅重新解析守衛之前被移除；一個鎖定檔案設定錯誤旗標所以所有權標記同登錄檔被保留用於重試。每個 `D|` 目錄條目（列在最深優先）用 `RMDir` 用最佳努力被移除，所以未知路徑保持佢哋非空父級。清單本身被最後移除。缺失清單會中止失閉同非零錯誤等級同冇檔案被移除。

固定祖先重新解析判斷（安裝根、程式家長、安裝目錄、開始選單根同軟件快捷方式目錄）喺兩個分支都首先執行，恰恰如同喺預製路徑上。

## 失敗模式

- **冇網絡、磁碟空間太少，或一個無法寫入嘅會話目錄** ： 前置檢查退出 30 喺任何工具被安裝或任何倉庫被克隆之前。
- **工具鏈安裝無法完成** ： 退出 10；冇被寫入固定安裝目錄。
- **克隆或檢出失敗**（壞標籤、網絡斷開） ： 退出 11。
- **opencode 無法被安裝**（npm 失敗、opencode 之後冇喺 PATH 上） ： 退出 12；構建無法冇佢自我修復。
- **構建永遠唔成功喺修復預算內，或暫存負載缺失其執行檔** ： 退出 20；用戶被提供預製退回。
- **意外協調器錯誤，或子過程失敗啟動** ： 退出 40 通過陷阱。
- **部分安裝嘅工具鏈保留** ： 工具安裝係機器可見同喺失敗時唔會回滾；一個稍後預製或源碼構建執行重用無論已經存在咩（每個工具喺安裝前被探測）。
- **會話目錄保留** ： 會話目錄（源碼克隆、依賴、構建輸出、暫存負載、`build.log`、`status.json`、`owned-manifest.txt`）係**唔**自動刪除。佢被保留用於診斷同可以係大；用戶可以手動移除佢。每個致命同有界失敗對話框報告其 `build.log` 路徑。

## 安全考慮

- **機器範圍安裝。** 源碼安裝開發者工具係機器範圍可見：Git、Node.js LTS、Visual Studio 2022 C++ 構建工具帶著相容 Windows 11 SDK（退回請求 `10.0.26100`）同 CMake。
  佢哋唔係限制到會話目錄同唔會喺失敗時或喺應用程式卸載時被移除。整個路徑喺用戶等級執行（安裝程式唔要求提升），儘管個別供應商安裝程式可能會自己提示提升。
- **從供應商 URL 嘅網絡獲取。** 啟動下載來自官方來源 ： `github.com/git-for-windows`、`nodejs.org`、`aka.ms/vs` 同 `github.com/Kitware/CMake` ： 加上 npm 登錄用 `opencode-ai`、來自 `PRODUCT_SOURCE_REPO_URL` 嘅 git 克隆同一個最佳努力模式獲取從 `opencode.ai/config.json`。每個退回安裝程式需要有效嘅 Authenticode 同一個精確嘅每供應商發佈者身份喺執行前。釘住嘅 Git、Node 同 CMake 項目亦到發佈者發佈中繼資料嘅精確每供應商 SHA-256 釘住。可變 Visual Studio 啟動程式唔係摘要釘住；其 Authenticode 鏈同精確 Microsoft 發佈者身份係信任大門。
- **精確源碼身份。** 發佈打包傳遞實時 GitHub 倉庫克隆 URL 同精確工作流程提交到每個真實同夾具 NSIS 編譯。安裝程式拒絕編譯冇該完整源碼提交，同協調器拒絕任何其他比 40 字元提交同驗證分離檢出喺任何修復或構建步驟前。冇可變分支或共享軟件版本標籤退回。
- **為咩 opencode 用倉庫嘅 `opencode.json` 權限執行。** 用於構建修復完全無頭，opencode 必須唔阻止喺批准提示上。排放器因此授予每個行動權限類別（`edit`、`bash`、`webfetch` 同任何未來行動類別實時模式列出）`allow`，同時保持 `question=deny`（一個互動請求自動拒絕代替掛起）同 `external_directory=deny`（opencode 無法編輯克隆外檔案）。配置係項目本地，所以毛氈允許係嚴格限制到克隆源碼目錄 ： 項目本地配置同 `external_directory=deny` 嘅組合係咩限制授予到克隆。
- **殘留風險（刻意表面）。** 喺克隆目錄內，opencode 執行帶著 `edit`、`bash` 同 `webfetch` 全部 `allow` 同冇互動大門。呢表示修復助手可以修改倉庫檔案同喺修復循環期間無人值守執行命令。呢係一個刻意貿易保持構建無頭；佢係被呼出所以殘留風險可以被減少（例如通過設定 `external_directory=allow` 只有如果一個人真正打算更廣闊範圍，或通過緊縮行動類別）冇改變機制。
- **互動式只同喺 CI 關閉。** 源碼係喺 `/S` 底下無法到達同永遠唔喺 CI 執行程式執行，所以以上冇網絡獲取或工具安裝發生喺自動化中。
- **非關閉視窗。** 視窗只喺活動構建期間鎖定（見「非關閉視窗」）。佢係一個 UX 保護反對留下部分安裝嘅工具鏈同半完成構建背後，同中止鈎子加禁用控制唔會否則改變特權或堅持；關閉喺每個終端狀態恢復。

## 驗證同 CI 劣化

源碼構建需要小時、實時互聯網訪問同機器範圍工具安裝，所以佢無法喺一個一次性 CI 執行程式執行同永遠無法喺 `/S` 底下到達。源碼路徑因此 **劣化到編譯同靜態檢查只** 喺 CI 中，呢個係有意圖，唔係一個差距：

- 現有「建立 Windows 安裝程式」步驟編譯真實 `.nsi`，呢個證明新安裝來源、構建進度同錯誤頁面同佢哋嘅函式編譯。
- `scripts/ci/Test-BuildFromSourceHelpers.ps1` 明確喺 Windows PowerShell 5.1 執行。佢解析每個助手帶著主機嘅預設解碼器，需要原生視覺腳本保持 ASCII 安全，拒絕無效 `cmake --build ... -DCMAKE_INSTALL_PREFIX` 形式，執行安全相對路徑退回，驗證 Node >=22 LTS/x64 同 CMake 版本探測（包括舊 LTS、更新 LTS 同未來 CMake 拒絕），執行有效、缺失、無效同不可信 Authenticode 中繼資料，證明 PATH 合併保持便攜式條目喺登錄檔添加前，同驗證重用 VS 2022 Community 軟件加完整 Windows SDK 檢測（包括缺失 UCRT 拒絕）反對夾具冇安裝或喺執行程式上依賴呢些組件。佢亦需要完整提交、分離檢出同檢出後身份比較。
- 周圍檢查亦對 `.nsi` 執行靜態 grep 用於安裝來源頁面、構建進度函式、靜默 -> 預製守衛、`InstallSource` 寫入、每個新雙語字串對同 `SC_CLOSE` 處理，加一個微小單位呼叫允許配置排放器判斷它排放 `question=deny` 同 `external_directory=deny`。
- 因為每個自動安裝程式執行使用 `/S`，靜默安裝記錄 `InstallSource=prebuilt`，呢個確認源碼留喺靜默模式外。

實際源碼構建喺 CI 中唔被執行。佢係喺驗證開發者機器上滿足前置檢查需求。冇安裝程式或無簽名應用程式被執行當準備呢文件時。
