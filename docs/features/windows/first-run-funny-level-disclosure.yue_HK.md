---
translation-of: first-run-funny-level-disclosure.md
source-sha256: 3e63a61e74eb0965a04dee5c278cb4f622895dca96e4e3fb914149ae06cfef0e
review-status: agent-drafted
---

> 英文原文：[First-run funny-level disclosure](first-run-funny-level-disclosure.md)

# 首次設定時講清楚搞笑程度

搞笑程度唔只係喺偏好設定入面先講，第一次設定 Bambu Studio 嗰陣已經會講清楚。首次執行嘅**設定嚮導**有一步叫**搞笑程度**，喺用戶繼續之前直接講明呢個設定做啲乜、顯示兩個語言而家嘅程度，仲可以即場更改或者重設任何一個。

## 行為

- **位置：** 設定嚮導入面，喺**請選擇登入地區**同使用者體驗改善計劃頁之間。嚮導喺第一次啟動時執行，之後亦可以由**說明 > 設定嚮導**再開，所以任何時候都可以返嚟睇呢一步。
- **按次序講明三件事：**
  1. 搞笑程度會影響 Bambu Studio 顯示嘅每一個訊息嘅語氣，**包括錯誤同警告**。佢只係改語氣：發生咗咩事、影響到咩、用戶可以點做，每個程度都會講得清清楚楚。
  2. 英文同廣東話各自有自己嘅搞笑程度，由 1（完全認真）到 5（玩到盡），**兩個都由第 5 級開始**。呢個數字由應用程式提供（`I18N::FUNNY_LEVEL_DEFAULT`），所以頁面永遠唔會講出一個同實際出廠唔同嘅預設值。
  3. 任何一個程度都可以**隨時更改或者重設**：而家喺呢一步，或者之後喺**偏好設定 > 一般**。
- **控制項：** 每種語言一條滑桿（**搞笑程度（英文）**、**搞笑程度（廣東話）**），各自讀出「第 N 級（共 5 級）」、顯示一條喺而家程度下嘅真實訊息（搞笑程度文案表入面嘅「Slicing complete」，用嗰條滑桿嘅語言），仲有**將英文重設返第 5 級**／**將廣東話重設返第 5 級**按鈕。移動滑桿，放手嗰陣就會儲存程度；重設會移除已儲存嘅值，等出廠預設值重新生效，同一個從來冇改過嘅設定檔完全一樣。兩條路都寫入同偏好設定滑桿一樣嘅鍵，亦會更新運行緊嘅語言服務，所以應用程式其他部分即刻跟住變。
- **改語氣，唔改事實：** 呢一步嘅開場白本身都會跟搞笑程度變（第 1-2 級一句、第 3 級一句、第 4-5 級一句）。上面嗰三點唔會因為程度而改變。
- **下一步同返回：** **下一步**會記低用戶已經睇過呢個說明，然後去使用者體驗改善計劃頁；**返回**會返去地區頁，而計劃頁嘅**返回**會返嚟呢一步。揀咗嘅地區照舊帶去計劃頁。

### 語言模式

所有文字都放喺嚮導嘅網頁文字目錄（`resources/web/data/text.js`，鍵 `t300` 至 `t313`），每個鍵都有英文同香港廣東話；頁面邏輯只係揀鍵同填入 `{default}`、`{level}` 佔位符。

- **英文**同**香港廣東話**模式用嗰種語言顯示呢一步。
- **雙語**模式每行用英文顯示，下面加一行廣東話，用嚮導本身嘅精簡第二行。開場白跟每種語言自己嘅程度：英文部分跟英文程度，廣東話部分跟廣東話程度。
- 嚮導用其他語言，或者某個鍵冇廣東話，就會退返用英文。

### 學校模式

學校模式開住嗰陣，搞笑程度當冇安裝咁處理。呢一步回覆頁面時唔會帶任何程度、預設值或者示範訊息，頁面乜都唔顯示，嚮導會跟用戶行緊嘅方向直接過去（向前去計劃頁，或者向後返地區頁）。學校模式開住時唔會寫入任何程度。

### 喺應用程式以外打開

用普通瀏覽器打開，頁面照樣顯示嗰三點同出廠預設值（第 5 級），但控制項會收埋，因為只有應用程式先可以儲存程度。

## 設定

| 鍵 | 位置 | 值 |
| --- | --- | --- |
| `funny_level_en` | `BambuStudio.conf`（AppConfig） | 1-5；冇呢個鍵即係出廠預設值 5 |
| `funny_level_yue` | `BambuStudio.conf`（AppConfig） | 1-5；冇呢個鍵即係出廠預設值 5 |
| `funny_level_disclosed` | `BambuStudio.conf`（AppConfig） | 睇完呢一步再撳**下一步**之後係 `true` |

## 實作

- `src/slic3r/GUI/FirstRunFunnyDisclosure.hpp`：唔依賴 wxWidgets 嘅模型。`payload()` 整出 `response_funny_disclosure` 回覆（程度、每個程度有冇儲存、示範訊息、範圍同預設值；學校模式下只有 `available: false`）。`parse_request()` 接受 `request_funny_disclosure`、`save_funny_level`（`language` 係 `en` 或者 `yue`，`level` 要係整數，會限制喺 1-5）、`reset_funny_level` 同 `acknowledge_funny_disclosure`，其他一律拒絕，所以乜都唔會寫入。
- `src/slic3r/GUI/WebGuideDialog.cpp`：`GuideFrame::OnScriptMessage` 將呢啲訊息交畀 `apply_funny_disclosure_request()`，佢會儲存或者移除鍵、更新 `I18N::language_mode_service()`、記低 `funny_level_disclosed`，再回覆頁面。佢嘅範圍用 `static_assert` 對準 `I18N::FUNNY_LEVEL_MIN`、`_MAX` 同 `_DEFAULT`。
- `resources/web/guide/12/`：呢一步嘅頁面（`index.html`、`12.css`、`12.js`）同唔用 DOM 嘅文案同路線邏輯（`funny-disclosure.js`）。`resources/web/guide/11/11.js` 而家會去呢一步，計劃頁嘅**返回**亦會返嚟呢度。

## 無障礙

滑桿係原生 range 輸入，有看得見嘅標籤、讀出嚟嘅值（經 `aria-valuetext` 讀「第 3 級（共 5 級）」）、禮貌模式嘅即時讀數，同埋指向示範訊息嘅描述。重設按鈕會講明重設邊種語言。淨係用鍵盤都用得晒（方向鍵移動滑桿，Space 或者 Enter 撳按鈕），亦沿用嚮導嘅焦點框、減少動態效果規則同淺色、深色顏色標記。

## 驗證

- `g++ -std=c++17 -Isrc -Itests -Itests/catch2 tests/language_mode/first_run_disclosure_tests.cpp`，再執行個程式：模型嘅預設值、搞笑程度兩個極端、學校模式，同每個接受同拒絕嘅請求（CTest 目標 `first_run_disclosure_tests`）。
- `node --test ui-md3/tests/first-run-funny-disclosure.test.mjs`：嚮導次序、頁面控制項、原生接線、目錄條目，同全新設定檔顯示出嚟嘅一步。之後再用英文、廣東話同雙語模式顯示呢一步，英文同廣東話程度分別係 1 同 1、5 同 5、1 同 5、5 同 1：每個模式都要用佢顯示嘅每種語言講齊三件事，第 1 級同第 5 級嘅事實要一字不差、只有開場白會變，每條滑桿要讀返自己嘅程度，亦唔可以留低未填嘅佔位符。退路都有測試：嚮導用其他語言或者冇廣東話條目時顯示英文；冇應用程式嘅頁面顯示第 5 級嘅事實但冇控制項；超出範圍嘅程度會被限制；學校模式會話呢一步唔適用。頁面邏輯唔可以自己帶任何文字，而反向回歸測試（講錯預設值、事實冇再提錯誤同警告、少咗廣東話一行、事實跟住程度變）每一個都一定要令檢查變紅。
- `node resources/web/data/validate-text-locales.mjs`：新鍵嘅廣東話對應。

仲要喺建置好嘅 Windows 應用程式入面親眼睇到：呢一步出現喺首次執行嚮導、滑桿同重設按鈕儲存到 `BambuStudio.conf`、示範訊息同開場白跟住滑桿變、學校模式開住時跳過呢一步，同埋每個語言模式喺窄闊度同高顯示比例下嘅版面。
