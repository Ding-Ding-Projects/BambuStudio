---
translation-of: windows-native-installer.md
source-sha256: b40df8dd1c1494f6eae23bff4d20c34510b73328916c02a8284b7f54851754d4
review-status: agent-drafted
---

> 英文原文：[Native Windows installer](windows-native-installer.md)

# 原生 Windows 安裝器

## 當前打包合約

支援嘅 Windows 安裝器係一個未簽名 Squirrel.Windows 發佈。打包腳本`scripts/windows/Invoke-SquirrelPackage.ps1` 建立一個 NuGet 包，包含已安裝原生C++ 有效負載喺 `lib/net45` 下，然後執行雜湊固定 Squirrel.Windows 2.0.1 `releasify` 命令。發佈目錄包含；

| 資產 | 目的 |
| --- | --- |
| `Setup.exe` | Squirrel 引導器；未簽名同埋可能觸發未知發布者或 SmartScreen 警告 |
| `RELEASES` | 帶包雜湊同埋檔案名嘅更新提供源索引 |
| `*-full.nupkg` | 完整安裝/更新包，包括 `bambu-studio.exe` |
| `*-delta.nupkg` | 由 Squirrel 生成嘅可選三角洲包 |
| `Setup.exe.sha256` | 本地可重現 SHA-256 副檔 |

包 nuspec 記錄確切 40 字符源提交同埋 HTTPS 版本庫 URL。包裝器驗證提供源引用、完整包內容、Setup.exe 有一個空 PE安全目錄（未簽名），同埋檢查和，喺手交資產到 CI 或手動發佈前。密碼簽名被永久禁用；出處同埋 SBOM 證書唔會使未簽名可執行文件簽名。

## 更新同埋安裝行為

Squirrel 擁有安裝同埋更新佈置喺用戶設定檔下。`Setup.exe` 係初始引導器；後續版本由 `RELEASES` 提供源同埋完整/三角洲包表示。應用程式保持可用，冇安裝器版本庫條目，同埋更新提供源係普通HTTPS 元數據加包雜湊。發佈永遠唔應該從檢查和單獨聲稱真實性。

一鍵包裝器唔會啟動 Setup.exe 除非 `-Install` 明確提供。佢寫所有生成資產喺源有效負載外，同埋拒絕繼續當有效負載係空嘅時、版本唔係數值、源提交唔係完整 SHA、版本庫唔係 HTTPS、包缺失 `bambu-studio.exe`，或生成 Setup.exe 包含 Authenticode憑證表。

## 安全同埋失敗邊界

- Squirrel.Windows 包從官方 NuGet 平面容器 URL 提取同埋檢查反對一個提交 SHA-256 固定喺提取前。
- 包路徑喺一個驗證暫時目錄中建立同埋複製只進請求`artifacts/windows/squirrel` 輸出目錄。
- `RELEASES` 必須引用生成完整包；包必須包含恰好一個 nuspec同埋原生可執行文件喺 `lib/net45/bambu-studio.exe`。
- 檢查和涵蓋 `Setup.exe` 同埋唔係一個簽名替代。用戶應該驗證佢喺啟動一個未簽名下載前。
- 冇憑證、私鑰、簽名服務或簽名憑據被請求或呼叫。

## 遺留 NSIS 源

`packaging/windows/BambuStudioMD3.nsi` 同埋佢嘅幫助腳本保持喺版本庫中作為歷史源舊發佈同埋審計記錄。佢哋唔係由當前構建或發佈被呼叫工作流。新安裝器工作必須目標上面嘅 Squirrel 合約；舊 NSIS UI 頁面同埋卸載程序行為唔係一個當前產品保證。

## 驗證

喺打包前執行本地合約檢查；

```powershell
pwsh -NoLogo -NoProfile -File scripts/ci/Test-OneClickBuild.ps1
pwsh -NoLogo -NoProfile -File scripts/ci/Test-WindowsRelease.ps1
```

喺構建後，CI 限制驗證器檢查真實 Squirrel 輸出喺一次性託管執行上。佢驗證 nuspec 中嘅確切源提交、`RELEASES`、完整包內容、SBOM 元數據、檢查和，同埋空 PE 安全目錄証明未簽名狀態。執行時 UI 捕捉保持一個分離證據邊界，同埋喺版本庫截圖矩陣中列出。
