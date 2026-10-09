---
translation-of: pdf-engine.md
source-sha256: 13f17ca7e8c47b78cf4aab27025b85e12aaadf79cc1c6554af89f661cb1cb3aa
review-status: agent-drafted
---

> 英文原文：[Bundled PDF engine](pdf-engine.md)

# 隨附 PDF 引擎

Windows x64 建置將官方 qpdf 12.4.2 MSVC runtime 放喺應用程式執行檔旁邊嘅 `tools/pdf/`。可信來源 manifest 係 `scripts/windows/local-pdf-tools.json`，固定版本封存 SHA-256、全部 10 個 runtime 檔案、74 個 SDK 檔案、授權文字位元組同元件中繼資料。未計 notices 同 manifest，runtime 檔案合共 9,113,552 位元組。下載二進位檔案唔會納入版本控制。

## 建置同打包整合

Configure `src/slic3r` 需要 `LOCAL_CONVERTER_QPDF_SDK` 指向呢個已驗證嘅 SDK，否則會停止並顯示錯誤，解釋點樣暫存。各建置途徑會喺 configure 之前自動暫存兩個目錄樹：

| 途徑 | 傳俾 configure 嘅 SDK | Runtime |
| --- | --- | --- |
| `build.bat`、`build-installer.bat`、`OneClickBuildInstaller.cmd` | `artifacts/local-pdf/sdk` | `install-dir/tools/pdf`（套件內容） |
| 託管 Windows 建置工作流程（`build_bambu.yml`） | `artifacts/local-pdf/sdk` | `install-dir/tools/pdf`（套件內容） |
| `build_win.bat` | `artifacts/local-pdf/sdk` | `artifacts/local-pdf/runtime`（要複製到工作程序旁邊嘅 `tools/pdf`） |

每條途徑都用 PowerShell 7（`pwsh`）執行呢個指令碼。一鍵啟動程序冇 PowerShell 7 時會自動安裝；`build_win.bat` 會停止並提示點樣安裝。手動 configure 前要自己暫存，就喺儲存庫根目錄執行：

```powershell
pwsh -NoProfile -File scripts/windows/Install-LocalPdfTools.ps1 `
  -Destination install-dir/tools/pdf -SdkDestination artifacts/local-pdf/sdk
pwsh -NoProfile -File scripts/windows/Install-LocalPdfTools.ps1 `
  -Destination install-dir/tools/pdf -SdkDestination artifacts/local-pdf/sdk -VerifyOnly
cmake -S . -B build -DLOCAL_CONVERTER_QPDF_SDK:PATH="$PWD/artifacts/local-pdf/sdk" <other options>
```

打包仍然要針對實際套件暫存目錄驗證。單靠呢個指令碼，唔能夠證明安裝程式包含呢啲檔案。SDK 提供 `include/qpdf/qpdf-c.h` 同 MSVC 匯入程式庫 `lib/qpdf.lib`。Runtime 包含 `qpdf30.dll`、`qpdf.exe` 同官方分發包內八個 Microsoft runtime DLL。只暫存匯入程式庫，唔暫存靜態程式庫。

啟動程序由官方 `qpdf/qpdf` 發佈版本、標籤 `v12.4.2` 取得確切封存。有 `GH_TOKEN`、`GITHUB_TOKEN` 或者已登入嘅 GitHub CLI 時，佢會用 `gh release download`；冇嘅話，或者嗰個下載失敗，就經 HTTPS 下載同一個發佈檔案，所以本機建置唔使登入 GitHub。無論邊個副本，都要同已記錄嘅大小同校驗碼相符，先至解壓明確允許清單內嘅檔案。`-Offline` 禁止下載，要求有效快取；`-VerifyOnly` 永遠唔下載或執行任何內容。已有資料嘅呼叫會驗證現有檔案。現有目的地不符會失敗，唔改內容。新目的地喺唯一相鄰目錄組裝，驗證後以一次目錄重新命名啟用。失敗暫存保留作診斷。唔刪除或取代現有目錄樹。版本更改必須暫存至全新目錄樹。

## 執行界線

應用程式只可以解析自己套件內 `tools/pdf` 目錄，按可信編譯固定值驗證所有 runtime 檔案，並用受限制 DLL 搜尋載入確切 DLL。旁邊自行撰寫嘅 manifest 唔係信任錨點。PATH 或開發者安裝都唔可以啟用 PDF 工具。執行期間轉換器唔可以到達建置時下載路徑。

qpdf 解析同轉換 PDF 結構，唔會光柵化頁面、提供 OCR、驗證數碼簽署，亦唔保證視覺呈現完全一樣。轉接器必須列明限制、拒絕不支援嘅不透明功能，並施加自身頁數、位元組、記憶體、CPU、時間同輸出限制。使用者文件位元組只可以進入已宣告隔離工作程序。套件驗證唔等於沙箱驗證。加密文件、簽署、嵌入操作、附件同表格，處理前需要明確轉接器功能決策。qpdf 警告唔係成功驗證結果。

## 授權同來源

Manifest 嵌入官方 qpdf 原始碼封存內未改動 Apache-2.0 `LICENSE.txt` 同 `NOTICE.md`（SHA-256 `8a58af5b6141319287c1883bec8bd1bd545b7567b7fc5e6ce5d25a1c85f36397`）。zlib 1.3.2#2、libjpeg-turbo 3.2.0#1 同嵌入 OpenSSL 3.6.4#1 notices，來自相符版本 `vcpkg.zip`（SHA-256 `82005252fee032135d07c85ad7626a38f26fb253c1e098d1d185f315bea15ebb`）嘅 `installed/x64-windows-static/share` 版權紀錄。呢啲封存用作建立來源；一般建置只下載 runtime 封存。元件清單包括 Microsoft Visual C++ Runtime 14.51.36247.0，以 `LicenseRef-Microsoft-Visual-Studio-Redistributable` 標示，附官方授權條款 URL。Microsoft DLL 再分發仍受適用 Microsoft 授權約束；qpdf Apache 授權唔會授予 Microsoft 元件權利。套件包含 `sbom.cdx.json`，係附 runtime 檔案 SHA-256 嘅 CycloneDX 1.6 元件清單，唔代表完整二進位組成認證。元件版本按上游套件中繼資料記錄。

## 驗證

`pwsh -NoProfile -File tests/local_pdf_package/verify.ps1` 測試由已驗證快取首次暫存、重用、SDK 同 runtime 驗證、損壞 runtime、缺少 notice、額外 DLL、被改 manifest、缺少離線快取、損壞封存，以及還原後成功驗證。佢喺 PATH 只有 Windows 系統工具嘅環境，對自有、冇內容 PDF 執行套件執行檔，核對確切版本、旋轉頁面、重開輸出，再驗證頁數同旋轉。呢啲檢查唔讀取使用者文件，亦唔證明應用程式沙箱、安裝程式包含性、介面整合或完整 PDF 操作目錄。

官方行為參考：[Running qpdf](https://qpdf.readthedocs.io/en/12.4/cli.html)。
