---
translation-of: md3-native-ui.md
source-sha256: fe61c1f660051ccc6211e62217fe7e8217dd5526048dbd2b9c6208362a33776e
review-status: agent-drafted
---

> 英文原文：[Native Material Design 3 UI on Windows](md3-native-ui.md)

# Windows 上嘅本地 Material Design 3 UI

## 行為

本地 wxWidgets 應用程式使用來自 `src/slic3r/GUI/Widgets/MD3Tokens.hpp` 同埋 `StateColor.hpp` 嘅語義 Material Design 3 顏色令牌。`StateColor.cpp` 將呢啲淺色令牌對應到深色調色板。重寫涵蓋應用程式 chrome、自訂組件、主頁、墨水管理器同埋基於 React 嘅裝置頁面，同時保留資料承載顏色，例如墨水樣本同埋打印機狀態指示燈。

Roboto Regular、Medium 同埋 Bold 被安裝為應用程式資源同埋喺 Windows 上私下登記。佢哋唔修改用家嘅系統字體收集。粵語同埋傳統 CJK 模式偏好 Microsoft JhengHei UI，因為 Roboto 唔包含呢啲字形。

## 配置

現存 Bambu Studio 外觀設定控制淺色同埋深色模式。主題變更會喺語義顏色被解析之前更新全球 `StateColor` 模式，然後重新繪製本地組件樹。應用程式偏好繼續使用 Bambu Studio 嘅正常每用家配置目錄。

## 故障模式

- 缺失嘅語義深色對應項會退回到佢哋嘅淺色令牌，而唔係終止應用程式。
- 缺失或不可用嘅偏好字體會透過現存 HarmonyOS / 系統字體路徑退回。
- 裝置同埋雲網絡檢視可能喺聯網不可用時展示離線內容；本地 chrome 同埋本地切片保持獨立可測試。
- 發行嘅 Windows 二進制文件目前係未簽名嘅。Windows 可能會展示一個未知發佈者警告；用家應該喺執行安裝程式之前驗證發行版本校驗和。

## 安全考慮

安裝程式係按用家嘅，並只喺 `%LOCALAPPDATA%\Programs\Bambu Studio MD3` 下面寫入應用程式檔案。佢嘅卸載程式首先驗證固定安裝路徑，然後只刪除喺構建時列出嘅負載所有檔案，同埋只喺空時移除目錄。未知路徑被保留。實時目標接點 / 符號連結故障時會閉合，鎖定檔案會保持卸載登記可用以供重試。用家檔案同埋項目係喺安裝目錄之外，並唔會被卸載移除。

## 驗證

- 最後完全發行嘅基線，喺當前候選工作之前，係提交 `1f1ecb960`、GitHub Actions 執行 `29671557311`、發行版本 `md3-windows-v02.08.01.55-r6.1`。
- 該基線編譯、安裝、打包同埋發行嘅本地 Windows 負載。佢包含 `bambu-studio.exe`、`BambuStudio.dll`、資源同埋所有三份 Roboto TTF 檔案。
- 跨翻譯單元 `wxColour` 比較器迴歸保持被修復，透過將調色板查找保留喺 `StateColor.cpp` 入面；未使用嘅公開 `GetDarkMap()` 訪問器而家已被移除。
- 候選工作流添加一個守衛本地淺色 / 深色 / 語言捕獲門禁，但佢嘅第一份成功證據執行同埋人類審查仍然待機。冇本地未簽名二進制文件被執行。

語言行為同埋佢仍然部分本地覆蓋是被記錄喺
[英文、香港粵語同埋雙語模式](language-modes.md) 裏面。捕獲邊界同埋佢嘅限制係被記錄喺 [本地 Windows 視覺煙霧測試](native-visual-smoke.md) 裏面。
