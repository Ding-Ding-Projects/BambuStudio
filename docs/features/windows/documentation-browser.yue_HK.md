---
translation-of: documentation-browser.md
source-sha256: f30fb0783e12174695846fb286278593800218cc7831383b4e9886f61fcc266b
review-status: agent-drafted
---

# 離線文件瀏覽器（F1）

在主視窗按 `F1`，或使用「說明 > 文件」，即可開啟原生離線文件瀏覽器。
指令面板的文件項目會在面板關閉後開啟對應文章，不會再直接跳到網上原始碼。

## 操作

- 左邊可搜尋文章標題及內文，並使用正規表達式開關及建立器。
- 類別樹列出目前打包的功能文章，右邊顯示文章內容。
- `Alt+Left` 及 `Alt+Right` 返回或前進，`Ctrl+F` 聚焦搜尋，`Esc` 關閉。
- 廣東話模式優先顯示 `.yue_HK.md` 文章，未翻譯文章則顯示英文。
  英文及雙語模式顯示原本英文文章。
- 文章內的相對文件連結在瀏覽器內開啟；外部網址經正常警告流程交由外部瀏覽器開啟。

## 打包與限制

原生建置會從目前 `docs/features/` 產生 `resources/docs/bundle.json`，並複製文章引用的本機圖片。
產生的檔案屬於建置輸出，不會提交到版本歷史。沒有網絡也可閱讀已打包文章。
遠端圖片及不支援的網址協定會被阻擋。原始 HTML 只會以文字顯示。
若 WebView2 無法建立，文章欄會顯示明確提示，搜尋及類別樹仍可使用。

本次快速交付只恢復及接駁原生程式碼，沒有執行測試、lint、啟動介面或截圖。
正式建置及安裝包狀態由發佈負責者記錄，不能把原始碼恢復當成介面驗證完成。

## 完整介面及打包合約

`DocsBrowserDialog`（`src/slic3r/GUI/DocsBrowserDialog.{hpp,cpp}`）係非模態、單一實例嘅 Material Design 3 視窗。主視窗任何位置嘅 F1、Help > Documentation，以及指令面板（Ctrl+Shift+F）每個 Documentation / ... 項目都會開啟佢。

`cmake/modules/BundleDocs.cmake` 用 CMake 腳本模式打包全部 `docs/features/**/*.md`，包括各類別 README.md，唔需要 Node 或 Python。圖片按 docs 相對路徑放入 `resources/docs/assets/`，所以 `../../screenshots/pages/x.png` 繼續有效。原生 GUI 目標依賴文件包產生，現有資源安裝流程包含結果。`docs_bundle` 係 `libslic3r_gui` 嘅依賴。手動重建命令：

```text
cmake -DDOCS_SOURCE=docs -DDOCS_OUTPUT=resources/docs -P cmake/modules/BundleDocs.cmake
```

只提交文章，唔提交產生嘅 resources/docs 目錄。搜尋及歷史只保留喺目前視窗，冇額外設定。

## 文章呈現及導航

共用 `src/libslic3r/Markdown.{hpp,cpp}` 係確定性、唔依賴 wx 嘅 CommonMark 子集，支援 ATX 標題及穩定 slug、段落、強調、行內程式碼、圍欄及縮排程式碼、排序／非排序／工作清單、引用、GitHub 對齊表格、連結、圖片及自動連結。Setext 標題、參照式連結、註腳同 HTML entity 未支援，會按字面顯示。原始 HTML 一律轉義。CSS 由即時 MD3 顏色角色產生，跟隨主題及暗色模式。

左側 SearchField 預設純文字，可切換 .* 正規表達式、tune 完整建立器，以及大小寫／整字／多行旗標，按標題及內文篩選類別樹，狀態顯示 N of M articles match。右側有返回、前進、標題及經 SetPage() 載入嘅 wxWebView，使用同 Home 分頁一樣嘅 WebView2 執行環境。

相對 Markdown 連結解析為 `bambudocs://article/<path>#<fragment>`，內部開啟；片段連結捲到同頁標題。docs/features 以外嘅儲存庫檔案，例如根 README 或 Postman 集合，交由系統瀏覽器開啟儲存庫頁面。http(s)、mailto、target=_blank 同新視窗要求都經一般開啟瀏覽器警告流程。每次從樹、指令面板或連結開啟文章會加入視窗歷史；返回後開新文章會丟棄前進歷史。

Tab 次序係搜尋、樹、返回、前進、文章、關閉。控制項有 Search documentation、Documentation articles、Back、Forward、Article 無障礙名稱；文章有用標題命名嘅 main 地標，片段連結會移焦到標題。所有對話框外框字串經 _L()；文章按作者內容顯示，唔會機器翻譯。

## 失敗及安全邊界

缺少 bundle.json 時樹會留空，狀態顯示 No documentation bundle was found beside the application resources. 並記錄警告。格式錯誤同樣處理並記錄解析錯誤。已連結但未打包文章會開啟儲存庫頁面；缺少圖片會顯示瀏覽器佔位及替代文字，打包時有警告。WebView2 無法建立時顯示指向文件資料夾嘅提示，樹及搜尋仍然有效。

呈現器轉義 <、>、&、雙引號同單引號，只有自身產生嘅標籤可以進入頁面。`wxEVT_WEBVIEW_NAVIGATING` 攔截並否決 bambudocs 導航，只開啟包內路徑；http(s) 一律否決並經警告交到外部瀏覽器，嵌入視窗唔載入遠端內容。遠端圖片、不支援協定及遠端子資源由內容安全原則阻擋。file:// 只限呈現器產生嘅 resources/docs/assets 圖片，唔產生任意本機檔案文章連結。搜尋使用 BoundedRegex 嘅時限及模式長度限制。

## 交付同待驗證項目

今次從保留嘅離線瀏覽器實作恢復並接駁目前原生介面。F1 指令 ID 同外觀編輯器捷徑分開。現有構建會產生文件包，但先前保留版本嘅呈現器及打包測試冇喺今次恢復時匯入。快速交付未執行測試、lint、介面操作或擷圖；正式構建及打包狀態由發佈負責者記錄。

後續要離線開啟安裝版本、按 F1、搜尋 regex、開文章、跟內部連結、返回／前進、切換廣東話，以及核對外部連結警告、不支援協定同遠端圖片阻擋。呢啲係待做檢查，唔係今次證據。

相關文章：[指令面板](command-palette.md)、[正規表達式建立器](regex-builder.md)、[鍵盤及響應式無障礙介面](gui-accessibility.md)、[原生 Material Design 3 介面](md3-native-ui.md)。
