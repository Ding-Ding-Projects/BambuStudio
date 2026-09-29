---
translation-of: funny-levels-and-dialog-emojis.md
source-sha256: 72c3a9ed3b6262642674eafcba8d91785d8ed345aae76636b0b2a1539b5ec353
review-status: agent-drafted
---

> 英文原文：[Funny levels and dialog emojis](funny-levels-and-dialog-emojis.md)

# 玩樂程度和對話框表情符號

## 行為

兩個獨立嘅 **玩樂程度** 滑桿和一個 **喺對話框和訊息方塊中顯示表情符號** 開關放喺 **偏好設定 ▸ 一般** 中，直接喺語言選擇器下方。

| 控制項 | 範圍／數值 | 預設值 | AppConfig 鍵 |
|---|---|---|---|
| 玩樂程度（英文） | 1（完全認真）… 5（最大玩樂） | 2 | `funny_level_en` |
| 玩樂程度（廣東話） | 1 … 5 | 2 | `funny_level_yue` |
| 喺對話框和訊息方塊中顯示表情符號 | 開／關 | 關 | `dialog_emojis` |
| 首次執行披露已顯示 | 內部標誌 | 未設定 | `funny_level_disclosed` |

程度改變訊息嘅 **語氣**，永遠唔會改變其 **事實**。每個變體保持同源字串相同嘅 printf 佔位符，因此檔案名稱、計數和按鈕執行嘅動作喺第 5 級和第 1 級完全相同。兩個階梯係獨立嘅：英文可以保持完全直白，廣東話係最大音量，完全如同文件網站（`ui-md3/site/copy.js`），此功能鏡像其階梯語義：

- 5 個變體一對一映射到級別 1–5。
- 3 個變體：級別 1–2 使用第一個，級別 3 使用第二個，級別 4–5 使用第三個。
- 2 個變體：級別 1–2 使用第一個，級別 3–5 使用第二個。
- 1 個變體：每個級別嘅同一文字（控制標籤、專有名詞）。

### 複製解析

`Slic3r::GUI::I18N::LanguageModeService::translate()`（參考 `src/slic3r/GUI/LanguageMode.cpp`）喺廣東話目錄之前諮詢複製表：

1. 英文主要係源字串有階梯時嘅活躍英文級別嘅英文階梯項目；否則係未改變嘅源字串。
2. 廣東話文字係當存在時嘅活躍廣東話級別嘅廣東話階梯項目；否則係策劃嘅 `yue_HK` 目錄項目；否則係英文。
3. 雙語模式組合兩者完全如同以前一樣（英文主要、廣東話次要）。

目前帶有階梯嘅字串：`Slicing complete`、`Export successfully.`、`Model file downloaded.`、`Setting saved: %s`、`Deleted: %s`、`Error`、`Warning`、`Unsaved Changes`、`Do you want to continue?`，加上功能自身嘅設定標籤作為單項目（級別不變）階梯，以便佢們無目錄項目而雙語呈現。表格中無任何項目喺每個級別呈現其基本字串。

### 對話框表情符號

當開關開啟時，`MsgDialog`（因此 `MessageDialog`、`ErrorDialog`、`WarningDialog`、`InfoDialog` 及其喺 `src/slic3r/GUI/MsgDialog.cpp` 中嘅子類別）會喺 **標題** 前加上一個非語義表情符號，從對話框嘅樣式標誌中選擇：

| 樣式 | 表情符號 |
|---|---|
| `wxICON_ERROR` | ❌ |
| `wxICON_WARNING` | ⚠️ |
| `wxICON_QUESTION` | ❓ |
| `wxAPPLY`（成功） | ✅ |
| 任何其他 | ℹ️ |

狀態含義保持喺 Material 標題圖像文字和複製上；表情符號係裝飾。佢永遠唔會出現喺按鈕、動作標籤、欄位標籤或可訪問名稱中：`add_button()` 和 `SetButtonLabel()` 將每個標籤通過 `strip_dialog_emoji()` 傳遞，因此即使呼叫者傳遞裝飾字串也會得到普通按鈕。重複裝飾已裝飾嘅標題係無操作，因此重複重新標題永遠唔會堆疊圖像文字。

### 首次執行披露

首次應用程式以任一級別上方 1 啟動時（編譯預設值 2 符合條件），`GUI_App::show_funny_level_disclosure_once()` 推送一個非阻止通知（`NotificationType::FunnyLevelDisclosure`、普通級別、自動淡化），說玩樂程度設定每個訊息嘅樣式（包括錯誤和警告），事實永遠唔會改變，設定住喺偏好設定 ▸ 一般。然後佢記錄 `funny_level_disclosed = true`，所以通知每個組態目錄觸發一次。佢永遠唔會阻止啟動或竊取焦點。

## 組態

- 兩個滑桿都係 Material Design 3 `Slider` 小工具（`src/slic3r/GUI/Widgets/Slider.hpp`）：鍵盤可聚焦、箭頭／頁面／首頁／結束可操作，並通過 `SliderAccessible` 暴露到輔助技術，其讀取用 `SetName()` 設定嘅行標題。
- 每行顯示 `Level N of 5`、一條 **來源行**（`Stored in BambuStudio.conf as N.` 或 `Not stored yet; using the compiled default 2.`）和一個 **此項改變什麼？** 文字按鈕，要求時顯示完整說明（漸進式披露）。標題清楚地陳述級別設定所有訊息嘅樣式，包括錯誤、警告和破壞性確認。
- 鍵刻意 **未** 由 `AppConfig::set_defaults()` 播種。缺少鍵意味著「編譯預設值有效」，呢個係使來源行真實嘅原因；鍵僅喺用戶移動滑桿或翻轉開關時寫入。
- 變更即時套用：滑桿寫入 AppConfig、儲存並更新處理中嘅 `LanguageModeService`，因此下一個 `translate_mode()` 呼叫和下一個對話框使用新值而唔需重新啟動。啟動時 `GUI_App` 喺語言模式配置後立即加載所有三個數值。
- 標籤跟足語言模式：英文、廣東話同雙語。雙語模式下，每行嘅 helper 將英文同粵語嗰一對記入雙語登記表，自己淨係顯示英文；之後由雙語裝飾器好似處理其他標籤咁配對：夠位就一行（`English · 廣東話`），嗰行會換行就英文喺上粵語喺下，唔夠位就將粵語放入工具提示。以前由呢幾行自己砌一行配對，會撐出捲動頁面，將每行嘅滑桿推出視線以外（版面裁剪清單 CJ-021）。

## 故障模式

- 畸形或超出範圍嘅永久儲存數值（`"loud"`、`"12"`、`"-2"`）永遠唔會到達 UI：`parse_funny_level()` 為非數字回退到編譯預設值並將數字夾住到 1–5。`set_funny_level()` 再次夾住，所以無代碼路徑能保持無效級別。
- 無階梯嘅源字串不受級別影響。無部分狀態：要麼整個階梯存在該語言，要麼使用基本複製。
- 如果廣東話目錄缺失，廣東話階梯項目仍然呈現（佢們唔依賴目錄）；所有其他都完全如 [語言模式](language-modes.md) 中所述回退到英文。
- 如果通知管理員喺 `post_init()` 時尚未可用，披露被跳過，標誌保持未設定，所以佢於下一次啟動時重試而不係無聲遺失。
- 對於空標題和開關關閉時，對話框表情符號被跳過；對話框永遠唔會顯示無文字嘅光圖像文字。

## 安全考量

- 無網絡、檔案或剪貼板存取涉及。複製表編譯入；無外部文字內插入。
- 格式參數（檔案名稱、計數）由呼叫者喺變體選擇後供應，因此變體永遠唔可以相對於源字串重新排序或刪除 `%s`/`%d` 佔位符：測試聲明每個階梯項目保留其佔位符。
- 表情符號裝飾限制於標題。可訪問名稱、按鈕標籤和任何可被讀回作為指令或標識符嘅文字保持普通。

## 驗證

- 捕獲（內置有效負載、隱藏桌面）：`docs/screenshots/md3-everything/preferences-general-funny-levels--en-light-comfortable--after.png`：兩個滑桿喺級別 2，帶有佢們嘅來源行和語言選擇器下嘅對話框表情符號切換。

Catch2 測試喺 `tests/language_mode/funny_level_tests.cpp`（目標 `language_mode_tests`）：

- `Funny levels clamp to the 1..5 range`
- `Persisted funny levels parse with a compiled default and stable keys`
- `Copy variants follow the level ladder per language`
- `Level variants keep their format placeholders so facts stay exact`
- `translate() picks the level variant and otherwise the base string`
- `Dialog emojis decorate headlines only and never action labels`

手動檢查：打開偏好設定 ▸ 一般，將 **玩樂程度（英文）** 移動到 5，然後觸發任何警告對話框（例如關閉帶有未儲存變更嘅項目）。標題讀取 `Unsaved changes are waiting for a decision`；按鈕保持其確切標籤。開啟 **喺對話框和訊息方塊中顯示表情符號** 並重新打開對話框：標題獲得前導 ⚠️，按鈕則不會。

## 建議文章

- [英文、香港廣東話和雙語模式](language-modes.md)：階梯插入嘅模式。
- [原生 Material Design 3 UI](md3-native-ui.md)：呢度使用嘅 Slider 和 Switch 小工具。
- [鍵盤、輔助和響應式 GUI 可訪問性](gui-accessibility.md)：滑桿和披露按鈕如何保持鍵盤和螢幕閱讀器可操作。
- [外觀客製化](appearance-customization.md)：相鄰嘅偏好設定分頁。
