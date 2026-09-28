---
translation-of: app-updates.md
source-sha256: 46d868054cb875615f1888c7194ddadf80687a1c29482a6c25c72004f5ab4b43
review-status: agent-drafted
---

> 英文原文：[App updates from this fork's releases](app-updates.md)

# 來自呢個分叉發佈嘅應用程式更新

## 行為

- **真實來源**：`Ding-Ding-Projects/BambuStudio` 嘅 GitHub 發佈（標籤 `md3-v<N>`），通過 `https://api.github.com/repos/Ding-Ding-Projects/BambuStudio/releases/latest` 讀取。Bambu Lab 雲源唔再為應用程式更新諮詢，因此應用程式永遠唔會提供用上游庫存構建替換自身（佢做：2.8.2.61 提示）。
- **新意味著稍後發佈**：發佈計算為更新，當其 `published_at` 比 `SLIC3R_BUILD_TIME` 晚超過三小時（喺構建主機上喺編譯時作為 `%Y%m%d-%H%M%S` 蓋章）。邊際吸收構建主機嘅時鐘偏移和編譯與發佈之間嘅分鐘。發佈編號未比較，因此比最新發佈新嘅本地開發構建永遠唔會被騷擾。
- **提供嘅**：發佈嘅 `Setup.exe` 資產（無資產列出時嘅發佈頁面）。對話框係 MD3 `UpdateVersionDialog`（`ReleaseNote.cpp`）：套件標題瓷磚、發佈名稱和註釋作為滾動正文中嘅文字、和下載／跳過此版本／取消頁腳藥丸。下載喺預設瀏覽器中打開資產。
- **跳過此版本** 儲存精確標籤喺 `app_config` `app/skip_version`；手動檢查（說明 ▸ 檢查更新）忽略跳過。
- **測試頻道**：`check_beta_version()` 係無操作；呢個分叉冇測試頻道。

## 組態

`enable_beta_version_update` 唔再改變行為。無其他設定涉及。

## 故障模式

- 無網絡、API 錯誤或畸形有效負載：自動檢查不顯示任何內容；手動檢查顯示「最新版本」通知而不係錯誤，原因被記錄。
- 無法解析 `published_at` 或構建時間：記錄，視為「無更新」。
- Squirrel 源排序：由 md3-v106 以來套件版本係 `2.8.<patch*1000+N>`（`2.8.2106`），其中 `N` 係發佈編號，因此 Squirrel 源更新程式正確排列發佈；md3-v104 和 md3-v105 兩者都帶有 `2.8.2-build61` 且由套件版本無法區別。

## 安全考量

- 公共 API 嘅匿名讀取；無令牌被發送。率限制（每小時每 IP 60 個請求）遠高於應用程式每次啟動一次呼叫加手動檢查。
- 安裝程式按政策係無簽署嘅；發佈註釋帶有 `Setup.exe` 嘅 SHA-256，應用程式將下載交給瀏覽器而不係提取和執行佢。

## 驗證

- 合約：`ui-md3/tests/md3-conversion-contracts.test.mjs`（`check_new_version` 讀取呢個分叉嘅發佈；測試檢查係無操作）。
- 執行時：使用比最新發佈舊嘅構建啟動並確認對話框命名 `md3-v<N>` 標籤並提供 `Setup.exe`；啟動比最新發佈新嘅構建並確認無對話框。

## 相關

- [Windows 原生安裝程式](../releases/windows-native-installer.md)
- [發佈代號稱](../releases/release-codenames.md)
