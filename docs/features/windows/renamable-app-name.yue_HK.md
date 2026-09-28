---
translation-of: renamable-app-name.md
source-sha256: 93413647a42ac1de2d4da8aa129cde59cea4ad8d798f71baf89bd284bdd19405
review-status: agent-drafted
---

> 英文原文：[Renamable app name (display label only)](renamable-app-name.md)

# 可重新命名嘅應用程式名稱（只係顯示標籤）

用家可以改變應用程式為自己展示嘅名稱。新名稱會出現喺自訂標題欄品牌標籤、任務欄 / 視窗標題、「關於」對話框標題、啟動螢幕標題、「幫助」選單嘅「關於…」項、「檔案」選單嘅「退出…」提示，同埋引入應用程式嘅共用訊息對話框嘅標題（「…錯誤」、「…警告」、「…資訊」、「…訊息」、「…，通知」、「…，持續上載」、「…，較新嘅 3mf 版本」）。

佢係一個**只係標籤**。冇嘢去識別軟件對作業系統、Bambu 服務、更新程式，或技術支援工程師嘅會讀取顯示名稱。

## 行為

- **喺邊度**：偏好設定 > 外觀 > **應用程式名稱**。
- **欄位**：一個 Material Design 3 文字欄，預先填入有效名稱。雖然冇儲存自訂名稱，佢會展示發運名稱（`Bambu Studio`，來自 `SLIC3R_APP_FULL_NAME`），呢個亦係欄位喺被清空時嘅提示。
- **提交**：<kbd>Enter</kbd> 或離開欄位。值會先行淨化（移除控制字符、修剪前後空格、內部空格執行摺疊成一個空格、截斷為 40 個碼位）同埋淨化文字會被回顯入欄位。
- **即時更新**：標題欄品牌標籤同埋視窗標題會立即改變；冇重新啟動。對話框標題同埋選單會喺佢哋下一次被構建時拿起新名稱。
- **重置為發運名稱**：清除儲存嘅值；訪問器會退回到發運名稱，欄位會展示佢。
- **說明**：喺**重新命名會改變咩嘢？**後面嘅標題會展開一份通俗嘅說明，講述重新命名做咩同埋唔做咩（漸進式揭示；行會讀成一個欄位，直到被問起）。
- **來源行**：喺欄位下面，要麼係 `儲存喺你嘅設定裏面（app_display_name）。發運名稱：Bambu Studio`，要麼係 `未設定。展示發運名稱 Bambu Studio。` 該行會命名真實嘅預設值，而唔係「預設」呢個字。

### 語言模式

標籤、標題、錯誤文字同埋來源行會經由 `_L()` 並遵循活躍語言模式（英文、香港粵語、雙語）同埋趣味級別，就好像其他「偏好設定」字串咁樣。發運產品名稱本身係一個品牌字串，並且**唔係**被翻譯嘅：佢喺每個模式都讀成 `Bambu Studio`，頂欄品牌標籤唔再將 `"Bambu Studio"` 經由目錄路由。

## 配置

| 鍵 | 位置 | 值 |
| --- | --- | --- |
| `app_display_name` | `BambuStudio.conf`（AppConfig） | 空或缺失 = 發運名稱；否則一份 1-40 碼位嘅 UTF-8 字串，冇控制字符 |

一個無效嘅儲存值（例如一個被手動編輯進設定檔案嘅）會被忽略：訪問器會退回到發運名稱，「偏好設定」欄位會喺打開時內嵌報告問題。儲存值永遠唔會被悄悄重寫。

代碼入口點：

- `Slic3r::GUI::AppDisplayName`（`src/slic3r/GUI/AppDisplayName.hpp/.cpp`）：純規則（`validate`、`sanitize`、`resolve`、`provenance`、`to_stored_value`）。冇 wxWidgets 或 libslic3r 依賴，所以規則係獨立進行單元測試嘅。
- `GUI_App::app_display_name()`：每份演示表面讀取嘅唯一訪問器。喺有效時返回儲存名稱，否則返回 `SLIC3R_APP_FULL_NAME`；喺 `app_config` 存在之前係安全嘅。
- `GUI_App::set_app_display_name(candidate)`：淨化、驗證、持久化（當結果等於發運名稱時 `""`），呼叫 `MainFrame::on_app_display_name_changed()` 然後喺應用程式物件上廣播 `EVT_APP_DISPLAY_NAME_CHANGED` 以供任何其他實時表面。
- `BBLTopbar::SetBrandLabel()`：重新標籤品牌標籤同埋重新符合項目芯片。

## 驗證規則

| 輸入 | 結果 |
| --- | --- |
| 空或只有空格 | `輸入 1 至 40 個字符嘅名稱，或重置為發運名稱。` |
| 超過 40 個碼位 | `太長：N 個字符。使用 40 個或更少。` |
| 換行符、製表符、DEL、C0/C1 控制（包括 NEL） | `移除換行符同埋其他控制字符。`、 被拒作為輸入；直到移除佢哋為止，冇嘢會被儲存 |
| `  My   Slicer  ` | 儲存為 `My Slicer` |
| 發運名稱，以任何間距 | 儲存為 `""` (與重置無法區別) |

長度係以 Unicode 碼位計算嘅，所以 40 個 CJK 字符或 40 個表情符號都係有效嘅。

## 故意保留真實產品名稱嘅嘢

呢啲讀 `SLIC3R_APP_NAME` / `SLIC3R_APP_FULL_NAME` / `SLIC3R_APP_KEY`，並被 `tests/app_display_name/app_display_name_identity_contract.cmake` 保護：

- 數據目錄同埋 `BambuStudio.conf` 路徑（`wxApp::SetAppName(SLIC3R_APP_KEY)`、`AppConfig::config_path`、`BambuStudio.cpp` 入面嘅 `--datadir` 預設）；
- 日誌檔案頭（`LogSink.cpp`）同埋 CLI 致命錯誤標題；
- G-code 頭、3MF `Application` 元數據同埋 G-code 生產者偵測；
- HTTP `User-Agent`、`X-BBL-Client-Name` 雲頭、Helio 客戶端名稱頭；
- 更新饋送日誌（`PresetUpdater.cpp`）同埋 Squirrel / 安裝程式身份（構建腳本、`.rc`）；
- 崩潰同埋診斷報告（`SendSystemInfoDialog.cpp`、`GUI_App.cpp` 入面嘅 `header_json["name"]` 診斷頭、`SysInfoDialog.cpp`，其文字被貼進錯誤報告裏）；
- 檔案關聯同埋登錄檔 ProgID 說明（`prog_desc = L"BambuStudio"`）；
- 臨時上載檔案名稱、已匯出嘅 G-code 轉譯器頭。

展示標題仍然攜帶常數，並喺呢輪被**唔係**轉換嘅（佢哋係在展示時構建嘅一次性對話框標題，列出嚟這樣空隙係一個決定而唔係一個遺漏）：`ConfigWizard.cpp`、`CreatePresetsDialog.cpp`、`Plater.cpp` 儲存 / 恢復 / 刪除標題、`Preferences.cpp` 儲存標題、`Tab.cpp`、`UserPresetsDialog.cpp`、`StatusPanel.cpp`、`AMSSetting.cpp`、`AMSMaterialsSetting.cpp`、`PhysicalPrinterDialog.cpp`、`PrintHostDialogs.cpp`、`UpdateDialogs.cpp` 同埋 `MainFrame` 設定對話框標題。佢哋可以逐個移進訪問器。

## 故障模式

- **無效輸入**：欄位保持輸入文字，狀態行會轉為錯誤角色並說明要改變咩；直到輸入有效為止，冇嘢會被儲存。
- **配置無法讀取 / `app_config` 缺失**：訪問器返回發運名稱。
- **超長粘貼**：控制會蓋住原始輸入於 200 字符，並內嵌報告 `太長`，而唔係中途剪裁；提交會截斷到 40 個碼位。
- **支援報告**：一個重新命名咗應用程式嘅用家仍然會產生話 `Bambu Studio` 嘅診斷，所以報告會命名軟件而唔係標籤。

## 安全考慮

- 控制字符（包括 C1 控制同埋 NEL）被拒同埋被剝離，所以儲存名稱永遠唔可以將換行符注入標題欄或一條引用佢嘅日誌行。
- 顯示名稱永遠唔會到達網絡頭、檔案名稱、登錄檔鍵或已匯出檔案；如果一份身份約束檔案開始引用鍵或模組，契約測試會令構建失敗。
- 該值存活喺用家自己嘅 `BambuStudio.conf`；佢唔會被同步或傳送。

## 驗證

- 截圖（構建嘅負載、隱藏桌面）：`docs/screenshots/md3-everything/preferences-appearance-app-name--en-light-comfortable--after.png`、 帶着其來源行嘅應用程式名稱欄、重置按鈕同埋喺自己行上嘅摺疊說明切換。

- `tests/app_display_name/app_display_name_tests`（Catch2）：邊界、碼位計數、控制同埋空格處理、淨化冪等性、退回、來源、儲存值摺疊。
- `tests/app_display_name/app_display_name_identity_contract.cmake`：手列身份檔案唔包含 `app_display_name` / `AppDisplayName` 引用；`GUI_App.cpp` 保持佢嘅身份行，訪問器退回到 `SLIC3R_APP_FULL_NAME`；頂欄、MainFrame、「關於」、MsgDialog、GUI.cpp 同埋「偏好設定」表面讀取訪問器；發運名稱唔會被傳過 `_L()`。
- 手動：喺「偏好設定 > 外觀」中重新命名，觀看品牌標籤同埋任務欄標題改變而冇重新啟動；打開「幫助 > 關於」同埋讀取標題；按**重置為發運名稱**；確認 `%APPDATA%\BambuStudio` 同埋日誌檔案名稱冇變。

## 建議嘅文章

- [外觀自訂](appearance-customization.md)、 呢個控制所在嘅選項卡。
- [英文、香港粵語同埋雙語模式](language-modes.md)、 標題係點樣被本地化嘅，而品牌字串唔係。
- [本地 Material Design 3 UI](md3-native-ui.md)、 使用嘅文字欄、按鈕同埋標題角色。
- [來自呢個分叉嘅發行版本嘅應用程式更新](app-updates.md)、 重新命名必須永遠唔接觸嘅更新程式身份。
