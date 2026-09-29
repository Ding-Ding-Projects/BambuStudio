---
translation-of: windows-one-click-build.md
source-sha256: 903e3a561ba695a011367e8aeda26e3d903dc0633ebe46f17c3dacf21b488dc8
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

工作流檢查至少 40 GB 嘅可用空間並安裝缺失嘅普通先決條件：Git 和 Git LFS、Visual Studio 2022 C++ 構建工具、完整 Windows SDK、CMake、Strawberry Perl 和 7-Zip。Strawberry Perl 當原生 `pkgconfiglite` 可執行檔不存在時供應 Windows `pkg-config.bat` 後援；構建明確匯出該包裝器，因此 CMake 唔會誤將 Strawberry 嘅無擴展幫手腳本誤以為可執行檔。現有支援嘅安裝會被重複使用。工具安裝無聲使用 `winget` 並接受套件／源同意；共用工具鏈幫手保留其發行商和引腳哈希檢查以供應商後援。依賴超級構建供應產品嘅哈希引腳 Node.js 和 pnpm 版本，因此工作流唔會替換無關係嘅系統 Node 安裝。

Squirrel.Windows 2.0.1 僅喺佢已喺用戶 NuGet 快取中可用時獲取。套件係從 NuGet 下載、根據提交嘅 SHA-256 引腳檢查、提取到用戶本地快取，然後由 `scripts/windows/Invoke-SquirrelPackage.ps1` 使用。唔接受無簽署指令、認證或簽署認證。

腳本獲取 Git LFS 物件、編譯依賴項、編譯 Release 應用程式、暫存 CMake 安裝有效負載、下載並驗證 CI 使用嘅相同哈希引腳 Mesa llvmpipe 後援、建立 CycloneDX SBOM、使用確切源提交和存儲庫後設資料建立 Squirrel NuGet 套件、執行 `Squirrel.exe --releasify`、驗證 `Setup.exe`、`RELEASES`、完整套件和 Setup.exe 上嘅空 PE 安全目錄（無簽署），然後寫入校驗和附屬文件。

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

## 故障模式和恢復

- 依賴項安裝可能需要 Windows 提升或重新啟動。喺批准應商安裝程式或重新啟動後重新執行相同指令；已完成嘅先決條件被偵測和重複使用。
- 清潔構建可能需要超過 40 GB 和數小時。成績單標識確切失敗嘅階段和退出碼。
- 網絡存取需要缺失套件、Git LFS 物件、引腳 Mesa 檔案和當佢唔快取時嘅哈希引腳 Squirrel.Windows NuGet 套件。
- 跟蹤嘅工作樹編輯可以本地編譯，但 Squirrel nuspec 僅可以記錄目前 Git 提交。當呢個使本地有效負載不可重現時工作流警告。
- 生成嘅 Squirrel 套件必須包含 `lib/net45/bambu-studio.exe`；缺少可執行檔、缺少 `RELEASES`、不匹配嘅校驗和或 Setup.exe 上嘅非空 PE 安全目錄失敗關閉。

沒有圖案、源內容或構建日誌被傳輸，除了聲明嘅套件、Git/LFS 和構建所需嘅引腳成品端點外。
