---
translation-of: README.md
source-sha256: 5d49ab519c0f3e05aefa037dc32b455515fe22227cda1c5e4e7ac2c23d84305e
review-status: agent-drafted
---

> 英文原文：[GitHub Pages site](README.md)

# GitHub Pages 網站

- [訪客設定排程](scheduled-settings.yue_HK.md)，本機臨時設定同明確配對限制。

發佈嘅網站 <https://ding-ding-projects.github.io/BambuStudio/> 係一個自我包含嘅靜態應用，構建自 [`ui-md3/landing.html`](../../../ui-md3/landing.html) 同埋 [`ui-md3/site/`](../../../ui-md3/site/) 中嘅模組。佢唔係一個帶滾動欄嘅行銷頁面：佢係一個瀏覽器風格嘅標籤表面，攜帶相同嘅責任如桌面應用 ， 三個語言模式、兩個有趣級別滑塊、每個搜尋欄後面嘅完整正則表達式構建器、非阻塞通知、一個完整更新日誌查看器同埋每元素外觀自訂。

代碼、字體同埋介面藝術被提供自呢個儲存庫。冇離網腳本或樣式表、冇 CDN、冇分析、冇追蹤器同埋冇餅乾橫幅；偏好生活喺訪問者自己嘅瀏覽器同埋其他地方冇。喺一個合格嘅重複訪問上，dim-sum 驚喜使一個映像要求到一個已發佈 release 資産喺公開 [`Ding-Ding-Projects/dim-sum-photos`](https://github.com/Ding-Ding-Projects/dim-sum-photos) 目錄帶 `no-referrer` 政策。

呢個聲稱現在被執行而唔係單純被陳述。`assert-pages-layout.mjs` 掃過每個已發佈頁面用於一個離網腳本或樣式表、同埋 `offline-render.test.mjs` 裝載組成網站喺一個無頭瀏覽器，帶每個離網主機無法到達，同埋要求佢無論如何渲染。兩者被添加喺 2026-07-28 之後、在 `/app/` 下設計系統 UI 套件被發現裝載 React、ReactDOM 同埋 `@babel/standalone` 從 unpkg ， 佢也意味著佢編譯佢自己嘅源喺訪問者嘅瀏覽器上每一次訪問同埋呈現冇任何嘢，當 unpkg 被阻塞或停止。見[部署同埋佈局門](deployment-and-layout-gate.md)用於點解兩個檢查工作。

## 功能

- [雙鑰匙破壞性確認](destructive-confirmation.yue_HK.md)：確切操作、兩個獨立鑰匙、完整滑桿、取消同單次執行。

- [本機個人用字](personal-wording.yue_HK.md)：只留喺此瀏覽器嘅 JSON 載入、更換、驗證快取同清除。
- [瀏覽器事件旁白](event-narration.yue_HK.md)：自選語言同聲音，順序播放，暫停同停止。
- [專注介面調整同訊息裝飾](attention-and-message-decoration.yue_HK.md)：五個預設關閉嘅獨立調整，同可持續儲存嘅裝飾表情符號開關。

- [標籤導航](tabbed-navigation.md) ， 帶、佢嘅溢出表面、重新排列、釘選、分組、可搜尋標籤列表同埋測量佈局演算法背後。
- [語言模式同埋有趣級別](language-and-funny-levels.md) ， 複製目錄、兩個獨立音調階同埋規則，分隔聲音來自事實。
- [正則表達式構建器](regex-builder.md) ， 共用組件、佢嘅引擎同埋方言、引導構造控制同埋有界評估，保留一個失控模式離頁面。
- [更新日誌查看器](changelog-viewer.md) ， 每個已發佈 release、日曆同埋輸入日期過濾、組成搜尋同埋 Markdown 匯出。
- [設定同埋外觀](settings-and-appearance.md) ， 主題、密度、主色調種子、排版、每元素編輯、設定搜尋同埋該網站上一個阻塞對話框。
- [通知](notifications.md) ， toast 棧、通知中心同埋哪些訊息被允許阻塞。
- [Dim sum 驚喜](dim-sum-surprise.md) ， 10% 啟動喜悅、佢嘅公開目錄照片同埋條件，喺佢保持安靜下。
- [部署同埋佈局門](deployment-and-layout-gate.md) ， 點解網站係組成、已發佈同埋被保留到 447 測量佈局情況，前一個部署被允許。

## 原型喺 `/app`

互動原型發佈喺 `/app/` 下係用設計系統文件，但兩個佢嘅合同係由呢個類別嘅測試執行，同埋屬於呢度：

- **每個裝飾圖示係 `aria-hidden`**。一個圖示字體連字被讀為字面文字、同埋喺一個僅圖示按鈕該文字變成可訪問名稱、遮蔽 `title` 意在命名佢。
- **每個佢嘅十個搜尋欄係接線嘅**，帶純文字作預設同埋搜尋欄嘅模式旅行，帶查詢。一個欄開啟一個正則表達式構建器同埋過濾冇嘢係比冇欄更差。

兩者、加原型嘅對話框語義同埋標題欄折疊合同、係斷言喺 `ui-md3/tests/layout-clipping.test.mjs` 同埋被捕捉喺 [`docs/screenshots/pages/app/`](../../screenshots/pages/README.md)。

## Postman

唔適用。呢個類別船隻冇 HTTP API：網站係靜態檔案。佢嘅可執行同埋風格資産係相同來源；一個合格 dim-sum 驚喜可能裝載一個公開目錄發佈映像。儲存庫嘅 HTTP 合同同埋佢哋嘅 Postman 集合生活喺 [`../api/`](../api/README.md)。

## 驗證

| 檢查 | 命令 |
|:---|:---|
| 數據同埋邏輯合同 | `node --test ui-md3/tests/site.test.mjs` |
| 行為合同（存儲、外觀、通知） | `node --test ui-md3/tests/site-behaviour.test.mjs` |
| 本地化執行時 | `node --test ui-md3/tests/i18n.test.mjs` |
| 靜態剪輯合同 | `node --test ui-md3/tests/layout-clipping.test.mjs` |
| 組成樹 | `node ui-md3/scripts/compose-site.mjs _site && node ui-md3/tests/assert-pages-layout.mjs _site` |
| 447 執行時佈局情況（156 著陸 + 288 每標籤 + 3 緊湊角表面情況） | `node ui-md3/tests/serve.mjs _site 4173 &` 然後 `BAMBU_PAGES_TEST_URL=http://127.0.0.1:4173/index.html node --test ui-md3/tests/runtime-layout-clipping.mjs` |
