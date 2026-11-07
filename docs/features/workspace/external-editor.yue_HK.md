---
translation-of: external-editor.md
source-sha256: df15243f5bdba04858cd80b8a64e91d3786655314b8b916551508aadbc430c23
review-status: agent-drafted
---

> 英文原文：[External editor](external-editor.md)

# 外部編輯器

「喺外部編輯器開啟」會使用你選擇嘅程式碼或文字編輯器，開啟目前專案所在資料夾，提供類似 GitHub Desktop 嘅外部編輯器整合。

## 行為

- **檔案 ▸ 喺外部編輯器開啟** 位於版本歷史紀錄下方，開啟目前 `.3mf` 所在資料夾。專案尚未有檔案時，選項會停用。
- 編輯器偵測位於 `src/slic3r/Utils/ExternalEditor.{hpp,cpp}`。Windows 會讀取 HKCU、HKLM 同 WOW6432Node 嘅移除程式登錄資料，尋找 VS Code、VS Code Insiders、VSCodium、Sublime Text、Notepad++、Cursor、Windsurf 同 Zed。清單參照 desktop-material 嘅實作；找不到時再掃描 `PATH`，並將 `.cmd` 啟動檔解析到真正執行檔。macOS 檢查 `/Applications` 套件及 PATH；Linux 檢查常用位置及 PATH。偵測結果每個程序只建立一次並快取。
- `src/slic3r/Utils/Process.cpp` 嘅 `open_in_external_editor` 會非同步啟動編輯器。Windows 使用 `wxExecute(argv, wxEXEC_ASYNC)`，macOS 使用 `boost::process::spawn`。參數以陣列傳遞，路徑有空格亦可正常處理；關閉本程式唔會順便關閉編輯器。

## 設定

偏好設定 ▸ 一般 ▸ **外部編輯器**：

- 下拉選單列出偵測到嘅編輯器及**自訂…**。AppConfig 嘅 `external_editor` 儲存編輯器名稱，或 `custom`。
- **外部編輯器路徑**提供瀏覽按鈕，開啟執行檔選擇器。路徑儲存為 `external_editor_path`，只喺選擇自訂時使用。

明確選擇自訂後，如果路徑留空，程式會顯示通知，提示你前往偏好設定，唔會自行改用另一個偵測到嘅編輯器。

## 失敗情況

- **未找到或未設定編輯器**：顯示「No external editor is configured…」一類角落通知，唔會崩潰或彈出對話框。
- **偵測後移除了編輯器**：非同步啟動會失敗。由於偵測結果按程序快取，新安裝或移除嘅編輯器要下次啟動本程式先會更新，呢個係已知限制。
- **專案未儲存過**：冇可開啟嘅專案資料夾，選單會停用。

## 安全考慮

- 只提供喺登錄安裝位置、常用安裝路徑或使用者 `PATH` 找到嘅執行檔。自訂路徑由使用者透過檔案對話框選擇，並儲存喺本機 AppConfig。
- 專案資料夾路徑以單一 argv 元素傳遞，唔會拼接成殼層指令字串，避免特製路徑造成參數注入。
- 登錄資料只會被讀取，唔會修改。

## 驗證

外部編輯器瀏覽按鈕會保留自己嘅本地化無障礙名稱。2026 年 11 月 7 日已移除將名稱覆蓋成下載路徑按鈕名稱嘅重複指派。原始碼檢視確認修正；新版本嘅原生輔助技術驗證仍待完成。

- 歷史記錄顯示，獨立檢查修正自訂路徑默默改用其他編輯器嘅問題後，`libslic3r_gui` 編譯完成，0 個錯誤。
- 一般分頁相關介面影像記錄位於 `docs/screenshots/preferences/`。歷史影像唔代表上述新修正已通過實機驗收。

## 專用匯出交接

共用匯出對話框使用 `open_export_in_visual_studio_code(target_utf8)`，唔會改用一般偏好編輯器嘅後備流程。每次會重新偵測 stable 同 Insiders 安裝、檢查使用者及電腦安裝位置，亦支援 PATH 上嘅可攜執行檔。目標必須存在。參數以向量傳遞，並包含選項分隔符；資料夾會以新工作區開啟。

未安裝 Code 時，對話框會明確提供 Download VS Code 操作，開啟官方下載頁面。安裝後可以直接重試，唔使重新啟動本程式。輸出不存在或啟動失敗會喺對話框顯示；使用者選擇嘅一般編輯器設定唔會改變。此新操作仍待原生 GUI 編譯及互動證據，主控台匯出測試唔能夠驗證呢個介面。
