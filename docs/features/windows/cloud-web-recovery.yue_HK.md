---
translation-of: cloud-web-recovery.md
source-sha256: f57ade84fe29c09c760008690f088950a06606c32c5ffe404541a8a195047d9a
review-status: agent-drafted
---

> 英文原文：[Cloud web-page failure recovery](cloud-web-recovery.md)

# 雲網頁失敗恢復

## 行為

MakerWorld、MakerLab 同 Print History 唔再用舊版完整頁面 `disconnect.html` 文件替換可用頁面當雲端登入票證要求或導航失敗。最後可用頁面保持裝載。一個堅持非模態原生資訊欄識別失敗係否由網絡、雲認證、缺失路徑、安全連線驗證或暫時不可用服務造成。佢嘅鍵盤可訪問 **Retry** 操作重複失敗要求，同成功導航解除警告。

如果首個遠端負載失敗同冇可用遠端文件保留，本地主頁表面保持喺同一資訊欄下可見。其他本地導航因此保持可用同時雲係不可用。相容性斷開文件保持喺資源包進行舊呼叫者，但佢嘅重試控制係而家一個語義 44 像素按鈕同佢嘅錯誤區域被公告到輔助技術。

## 配置同數據處理

冇偏好或認證由呢個功能存儲。重試保持只有失敗 URL 或票證要求回調喺所有者 `WebViewPanel`；両個帶面板消失。錯誤詳情唔被發送到另一服務，同冇認證票證被寫到通知。

## 失敗模式

- 連線失敗報告網絡係不可用。
- 失敗票證或認證導航報告雲端登入失敗。
- `wxWEBVIEW_NAV_ERR_NOT_FOUND` 報告要求雲頁面未發現；佢係唔標籤錯誤作為網絡中斷。
- 證書同安全失敗使用安全連線措辭同保留當前頁面。
- 用戶取消導航保持靜靜同唔引起假錯誤。

警告保持可見直到解除或直到受影響 WebView 完成非空白導航。重試失敗更新同一欄而唔係堆積對話框或替換內容。

## 驗證

- `deviceweb_home_webview_failure_policy_tests` 執行每個政策類別。
- `deviceweb_home_webview_failure_contract` 證明原生集成冇 `MakeDisconnectUrl`/`LoadURL(disconnect.html)` 替換路徑，包括網絡/認證/未發現副本，安裝非阻止重試操作，同保留可訪問相容性標記。
- `node resources/web/data/validate-text-locales.mjs` 驗證打包主頁本地化表格，包括英語/粵語雙語網絡副本。
