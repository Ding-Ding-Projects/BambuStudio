---
translation-of: browser-download-capture.md
source-sha256: bd97e21fa1ece76b1b2cf12c60034251e51d8f9913a4482880f7a84d4e5758fa
review-status: agent-drafted
---

> 英文原文：[Browser download capture](browser-download-capture.md)

# 瀏覽器下載接手

`browser-extension` 資料夾入面係一個俾 Chromium 瀏覽器（Google Chrome 同 Microsoft Edge）用嘅 Manifest V3 擴充功能。瀏覽器開始下載 3D 模型嗰陣，擴充功能會暫停下載，再叫 Bambu Studio MD3 接手。Bambu Studio MD3 要回覆話已經將下載排咗隊、等佢嘅「開始下載」對話框處理，瀏覽器先會取消自己嗰份；嗰個對話框會顯示檔案、來源同目的地，你確認之前乜都唔會傳送。其他回覆、或者冇回覆，瀏覽器都會照平時咁完成下載。應用程式嗰邊未喺呢個版本入面，詳情睇下一節。

## 呢個版本有啲乜

- 擴充功能、設定頁、接手規則同交接訊息，有英文、香港廣東話同兩樣一齊顯示。
- Windows 安裝會將擴充功能放喺 `bambu-studio.exe` 旁邊嘅 `browser-extension` 資料夾。

交接嘅應用程式部分未喺呢個版本入面：瀏覽器連接（原生訊息主機，同埋佢喺 Chrome 同 Edge 嘅登記）、「開始下載」對話框、「下載緊」視窗同「下載完成」通知。未有呢啲之前，**檢查連接**會報告瀏覽器連接未安裝，每個被接手嘅下載都會喺擴充功能記低原因之後繼續由瀏覽器下載。唔會漏失任何下載，亦唔會聲稱已經交接咗。

## 安裝擴充功能

1. 喺 Chrome 開 `chrome://extensions`，或者喺 Edge 開 `edge://extensions`。
2. 開啟**開發人員模式**。
3. 揀**載入未封裝項目**，再揀 `browser-extension` 資料夾。已安裝嘅版本喺 `%LOCALAPPDATA%\BambuStudioMD3\app-<version>\browser-extension`。每次更新都會裝一個新嘅 `app-<version>` 資料夾，之後再移除舊嗰個，所以請將資料夾複製去一個長期位置再載入嗰份副本，或者更新之後再載入一次。

擴充功能 ID 一定係 `beapempohkjjpcjdfojdlngcbamofiok`。manifest 帶住一條公開金鑰，所以每個瀏覽器都會得出同一個 ID，而瀏覽器連接會將呢個 ID 列為唯一准許嘅來源。擴充功能唔可以喺私密視窗運行（`"incognito": "not_allowed"`）。

## 開始下載嗰陣會點

Service worker 會聽 `chrome.downloads.onDeterminingFilename`。有監聽器未放行呢個事件嘅時候，瀏覽器完成唔到下載，所以會喺儲存任何檔案之前作出決定。

1. **決定。** 以下條件全部符合先會接手：接手開咗；下載進行緊；唔係私密視窗；唔係其他擴充功能開始嘅下載；地址係 `http` 或者 `https`、冇用戶名稱或者密碼、最多 8,192 個字元；網頁嘅網站同檔案嘅網站都冇被排除；檔名結尾係開咗嘅檔案類型。檔名冇已知結尾嘅話，就睇模型 MIME 類型（`model/3mf`、`model/stl`、`model/step`、`model/obj` 之類）；`application/octet-stream` 永遠唔算數。其他下載會即刻放行，唔留低任何記錄。
2. **暫停。** 送出任何嘢之前，會先暫停瀏覽器自己嘅傳送。瀏覽器唔肯暫停嘅話，乜都唔會送出，下載留喺瀏覽器。
3. **交接。** 擴充功能開一個去 `io.github.ding_ding_projects.bambustudio_md3` 嘅原生訊息連接埠，送出一個接手訊息（見下面）。
4. **排咗隊。** 淨係同一個接手嘅 `queued` 回覆，先會令擴充功能取消瀏覽器嘅下載，同埋喺瀏覽器下載列表移除佢。之後由 Bambu Studio MD3 負責呢個下載，傳送任何嘢之前一定要先問你。
5. **留喺瀏覽器。** 瀏覽器連接唔存在或者拒絕、接手被拒絕、回覆講緊另一個接手、回覆睇唔明、或者 30 秒內冇回覆，都會令瀏覽器繼續傳送。瀏覽器會照平時咁完成（如果你開咗「另存為」問題，都會照問），擴充功能會顯示通知，講明檔案同原因。

擴充功能自己永遠唔會開始傳送，亦永遠唔會送出 cookie、網頁內容或者網頁嘅查詢字串。Bambu Studio MD3 會自己下載個地址，唔會用瀏覽器嘅 cookie 或者登入狀態，所以只肯將檔案俾已登入瀏覽器嘅網站，或者簽署地址好快失效嘅網站，喺應用程式嗰邊可能會下載失敗。將呢啲網站加入排除列表，佢哋嘅下載就會留喺瀏覽器。

### 連結選單

對住地址結尾係模型檔案類型嘅連結㩒右鍵，會有**用 Bambu Studio MD3 開呢條連結**。呢個明確要求唔理接手開關、檔案類型選擇同排除網站，但照樣檢查地址。交接唔到嘅連結一定會顯示通知，唔理通知設定點揀。

## 設定頁

㩒擴充功能嘅工具列按鈕，或者揀**擴充功能選項**，就會開設定頁。每個改動都會即刻儲存，並喺頁面嘅狀態行讀出。

| 設定 | 預設 | 效果 |
| --- | --- | --- |
| 將模型下載交俾 Bambu Studio MD3 | 開 | 關咗就所有下載都留喺瀏覽器，工具列按鈕會顯示「關」標記 |
| 要交接嘅檔案類型 | 3MF、STL、STEP、OBJ、AMF 同 OLTP 開；G-code、SVG、glTF 同 FBX 關 | 網站都會為其他程式提供圖樣、場景同 G-code，所以呢啲一開始係關咗 |
| 以下網站嘅下載永遠唔好交接 | 空白 | 每行一個網站；`*.example.com` 包括嗰個網站同佢下面所有地址；貼上嘅地址會淨低網站；唔係網站嘅行會逐行講明，唔會儲存 |
| 瀏覽器繼續處理下載嗰陣通知我 | 開 | 自動接手但留喺瀏覽器嘅下載會有通知 |
| 擴充功能用嘅語言 | 跟瀏覽器 | 英文、香港廣東話，或者英文下面加廣東話 |

**連接**部分會顯示瀏覽器連接名稱同擴充功能 ID，附複製按鈕；**檢查連接**會叫瀏覽器連接回覆。**最近交接**列表喺瀏覽器本機擴充功能儲存空間保留最近 25 個決定（時間、檔案、網站、結果同原因）；**清除列表**會清空佢。

頁面會跟系統嘅淺色或深色外觀、高對比色彩同減少動態設定；窄視窗會變成單欄；每個控件都有看得見嘅標籤同鍵盤焦點框。接手開關會以開關讀出。

### 語言

Chrome 同 Edge 會按自己嘅介面語言揀擴充功能語言，但兩個都冇廣東話，所以擴充功能會自己讀兩份目錄（`_locales/en` 同 `_locales/yue_HK`），跟設定頁嘅選擇。**跟瀏覽器**喺廣東話或者香港中文瀏覽器用廣東話，其他用英文。呢個選擇會用喺設定頁、連結選單、工具列標題同標記，同埋通知。瀏覽器嘅擴充功能列表會顯示英文名稱同描述。

## 權限同私隱

擴充功能要求 `downloads`、`nativeMessaging`、`contextMenus`、`notifications` 同 `storage`，唔要求任何網站存取權。佢唔會由網絡載入任何嘢，冇分析追蹤，亦唔會執行生成出嚟嘅程式碼。設定同最近交接列表留喺瀏覽器設定檔嘅 `chrome.storage.local`。

## 交接訊息

每個訊息都係經 Chrome 原生訊息傳送嘅 JSON，每個連接埠一個訊息、一個回覆。

接手訊息：

```json
{
  "type": "capture", "protocol": 1, "captureId": "2b0f…", "origin": "download",
  "url": "https://cdn.example/files/benchy.3mf?sig=abc",
  "source": "https://models.example/item/123",
  "fileName": "benchy.3mf", "modelType": "3mf", "mime": "application/octet-stream",
  "totalBytes": 1048576, "capturedAt": "2026-10-09T08:30:00.000Z"
}
```

由連結選單嚟嘅話，`origin` 係 `link`。`url` 會移除片段。`source` 淨係網頁嘅協定、主機同路徑，或者 `null`。`fileName` 冇資料夾部分、冇控制字元、冇 `<>:"/\|?*`，最多 200 個字元；瀏覽器俾嘅名冇類型結尾嘅話會加返。唔知大小嗰陣 `totalBytes` 係 `null`。

回覆要講返同一個 `captureId`：

```json
{ "type": "capture-result", "protocol": 1, "captureId": "2b0f…", "status": "queued", "queueItemId": "…" }
{ "type": "capture-result", "protocol": 1, "captureId": "2b0f…", "status": "declined", "reason": "busy" }
```

`queued` 即係項目喺「開始下載」對話框後面等緊，乜都未傳送過。拒絕原因有 `busy`、`disabled`、`invalid`、`shutting-down` 同 `unsupported`；其他原因會當成原因不明嘅拒絕。

**檢查連接**會送出 `{ "type": "hello", "protocol": 1 }`，等 `{ "type": "hello", "protocol": 1, "app": "Bambu Studio MD3", "version": "…" }`。協定編號唔同會報告為版本唔夾。

## 驗證

- `node --test ui-md3/tests/browser-extension.test.mjs` 檢查 manifest、固定擴充功能 ID、圖示、冇網絡載入同生成程式碼、兩份目錄（鍵同替換位一致、寫成廣東話、冇多餘或者缺少嘅訊息）、每條接手規則、訊息、回覆，同埋用一個記錄住所有呼叫嘅 `chrome` API 代用品，檢查暫停、交接、取消同繼續嘅準確次序，包括逾時、拒絕、記錄同通知。
- `node --test ui-md3/tests/browser-extension-chromium.test.mjs` 將擴充功能載入真正嘅 Chromium，配一個只為嗰個測試設定檔登記嘅代用瀏覽器連接，再㩒真嘅下載連結。佢檢查排咗隊嘅模型唔會喺瀏覽器留低檔案；被拒絕或者冇連接嗰陣，瀏覽器會儲存完整檔案；其他下載永遠唔會去到連接；關咗接手就唔會郁模型；設定頁可以檢查連接同轉做廣東話。請設定 `CHROMIUM_PATH`，或者將 `PLAYWRIGHT_BROWSERS_PATH` 設為 Playwright 瀏覽器資料夾；兩樣都冇就會略過測試。品牌版 Chrome 唔理 `--load-extension`，所以唔會用。

兩個測試都冇行到 Bambu Studio MD3 本身。喺 Windows 嘅 Chrome 或者 Edge 載入擴充功能，同埋經應用程式「開始下載」、「下載緊」同「下載完成」畫面嘅完整流程，要等應用程式部分完成之後再實際觀察。
