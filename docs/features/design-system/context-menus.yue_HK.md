---
translation-of: context-menus.md
source-sha256: 579a315581cdc31a9b0efc0c57fe646d1a97bd66eff14c6bc4ca07e0ec8bc21e
review-status: agent-drafted
---

> 英文原文：[Context menus](context-menus.md)

# 右鍵選單

Windows 應用程式嘅每個右鍵選單都係 Material 選單（`src/slic3r/GUI/Widgets/MD3Menu.cpp` 入面嘅 `MD3::PopupMenu`）：
同標題列嘅選單係同一種介面，跟隨主題、密度同三種語言模式（英文、粵語，同雙語嘅「English · 廣東話」配對）。

## 喺邊度出現

| 介面 | 選單 | 經由 |
| --- | --- | --- |
| 3D 畫布、物件清單、打印板、預設組合清單、組裝步驟 | 物件、打印板同預設組合嘅操作 | `Plater::PopupMenu`（佢包住 `MD3::PopupMenu`） |
| 分頁列、標題列、輔助清單、裝置狀態、網頁工具 | 分頁、選單列同面板嘅操作 | 直接用 `MD3::PopupMenu` 或 `MD3::PopupMenuSelection` |
| 每個文字欄 | 復原、剪下、複製、貼上、刪除、全部選取 | 全程式通用嘅文字選單（見下文） |
| 可以複製嘅裝置標籤（打印機名稱、序號、版本） | 複製 | `enable_static_text_copy_menu` |
| 外觀編輯器接管咗嘅任何元素 | 編輯外觀... | 由 `MD3::PopupMenu` 加到上面每一個選單 |

## 文字欄

`MD3::EnableTextContextMenus(true)` 喺任何視窗出現之前由 `GUI_App` 呼叫，佢會安裝一個事件篩選器，回應每個文字欄嘅右鍵選單
請求：`wxTextCtrl`、套件 `TextInput`、`SpinInput`、`TempInput` 同 `SearchField` 入面嘅欄位、可以編輯嘅下拉選單，同搜尋同
富文本控件。篩選器自己處理請求，所以系統嘅編輯選單唔會打開，改為顯示 Material 選單，有復原、剪下、複製、貼上、刪除同全部
選取，每一項按欄位容許嘅情況啟用：

- 唯讀欄位只提供複製同全部選取；
- 遮蔽欄位（密碼，或者建立之後先遮蔽嘅智能家居令牌）永遠唔會提供剪下或者複製，所以秘密唔會經選單去到剪貼簿；
- 鍵盤請求（Menu 鍵或 Shift+F10）會喺欄位下面打開選單，滑鼠右鍵就喺指標位置打開。

唯讀下拉選單係清單而唔係欄位，保留自己嘅行為。喺外觀編輯器接管咗嘅元素上面按 Shift+右鍵，仍然會直接打開佢嘅編輯器。
程式關閉期間，會趁事件迴圈仲喺度嘅時候移除篩選器。

套件欄位以前會食咗右鍵點擊嚟收起系統選單，結果用滑鼠乜嘢選單都冇，用鍵盤就照樣打開系統選單。而家佢哋會將右鍵點擊交畀
Material 選單。

## 網頁

`WebView::CreateWebView` 會關閉瀏覽器自己嘅選單（返回、重新整理、另存新檔、檢查），除非開咗開發者工具設定；直接建立嘅
「沖刷體積」對話框網頁亦會關閉佢。內部構建可以為咗除錯打開佢（`#if !BBL_RELEASE_TO_PUBLIC`）。

## 驗證

- `node --test ui-md3/tests/context-menus.test.mjs` 檢查：除咗 Material 選單之外冇任何 `wxWindow::PopupMenu` 呼叫；文字
  選單嘅篩選器、項目、遮蔽同鍵盤打開嘅位置；佢喺啟動時安裝、關閉時移除；套件欄位嘅右鍵點擊；網頁嘅選單設定；同
  「新功能 / 更新日誌」嘅年份欄（用套件 `SpinInput` 代替原生數字調節欄，舊嗰個會畫系統方框，仲會打開系統編輯選單）。
- `scripts/md3/check-context-menus.py` 喺隱藏桌面用發佈套件執行：打開智能家居同偏好設定，分別用右鍵點擊同鍵盤請求要佢哋
  嘅文字欄打開選單，擷圖打開咗嘅嘢，並記錄佢嘅視窗類別。Windows 自己畫嘅選單視窗類別係 `#32768`；出現一個，或者請求乜都
  冇打開，執行就會失敗。喺 `md3-v162`（呢次改動之前），智能家居嘅網址欄同偏好設定嘅搜尋欄，右鍵點擊乜都冇打開，鍵盤請求
  就打開系統嘅英文編輯選單
  （`docs/screenshots/md3-everything/context-menu-smart-home-keyboard--en-light-comfortable--md3-v162.png`）。

視窗標題列保留 Windows 嘅系統選單（Alt+Space）；佢屬於視窗框，唔屬於程式嘅內容。
