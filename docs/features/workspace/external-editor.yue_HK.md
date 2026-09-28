---
translation-of: external-editor.md
source-sha256: 8e46e044c9cc60783210d559e9bbc4e1a324dca0baded441ad9075bfa5200e06
review-status: agent-drafted
---

> 英文原文：[External editor](external-editor.md)

# 外部編輯器

「喺外部編輯器開啟」開啟目前項目嘅資料夾喺一個使用者選擇碼/文字編輯器，鏡像 GitHub Desktop 嘅外部編輯器整合。

## 行為

- **檔案 ▸ 喺外部編輯器開啟**（喺版本歷史紀錄下方）開啟資料夾，包含目前項目嘅 `.3mf`。禁用，當冇項目檔案存在時。
- 編輯器檢測（`src/slic3r/Utils/ExternalEditor.{hpp,cpp}`）：探測 Windows 移除登錄檔鍵（HKCU + HKLM，包括 WOW6432Node），對於 VS Code、VS Code Insiders、VSCodium、Sublime Text、Notepad++、Cursor、Windsurf 同 Zed（表轉謄自桌面物料參考實作），然後回到掃描 `PATH` 對於編輯器嘅 CLI 裏面（`.cmd` 裏面解析到佢哋嘅真實執行檔）。macOS 探測 `/Applications` 束加上 PATH；Linux 探測知名路徑加上 PATH。檢測執行一次每處理程序同被快取。
- 啟動（`open_in_external_editor` 喺 `src/slic3r/Utils/Process.cpp`）：分離非同步生成（Windows 上 `wxExecute(argv, wxEXEC_ASYNC)`、macOS 上 `boost::process::spawn`）、argv 基於，所以帶著空格嘅路徑係安全嘅；編輯器活得比應用程式更久。

## 設定

偏好設定 ▸ 一般 ▸ **外部編輯器**：

- 偵測編輯器加上**自訂…**嘅下拉 ， 保留作為 AppConfig `external_editor`（編輯器名稱，或 `custom`）。
- **外部編輯器路徑**列帶著瀏覽（執行檔選擇器）， 保留作為 `external_editor_path`；當下拉設定為自訂時被使用。

一個明確自訂選擇被嚴格尊重：如果佢嘅路徑係空嘅應用程式顯示一個警告敬酒指向偏好設定，而唔係默默啟動一個自動檢測編輯器。

## 失敗模式

- **冇編輯器發現/設定** → 警告角敬酒（「冇外部編輯器係配置…」）、冇崩潰、冇對話。
- **編輯器喺檢測後卸載** → 生成非同步失敗；檢測係每處理程序快取，所以新（移除）安裝編輯器被撿起喺下一個應用程式開始（記錄限制）。
- **未儲存/從不儲存項目** → 選單項目禁用（冇資料夾去開啟）。

## 安全考慮

- 只有發現執行檔，喺登錄安裝位置下、知名安裝路徑或使用者嘅 `PATH` 被提供；自訂路徑係使用者選擇通過檔案對話同儲存本地喺 AppConfig。
- 項目資料夾路徑作為單一 argv 元素傳遞（冇殼層字串內插），防止參數注入經由工藝項目路徑。
- 登錄存取係唯讀。

## 驗證

- 編譯清潔到 `libslic3r_gui`（0 錯誤），喺敵對檢查之後（檢查發現喺默默自訂備用固定）。
- UI 擷取喺截圖矩陣下 `docs/screenshots/preferences/`（一般分頁列），喺一次構建到執行 exe。
