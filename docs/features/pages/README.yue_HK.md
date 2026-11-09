---
translation-of: README.md
source-sha256: d5ed61a049230aa7e59d92d295fb0e415338c56beec34dd39239cff59680494d
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
- [部署同埋佈局門](deployment-and-layout-gate.md) ， 點解網站係組成、已發佈同埋被保留到 555 測量佈局情況，前一個部署被允許。

## 重新設計：一層一層

2026 年 11 月 9 日圍住網站本身已有、大家都鍾意嘅兩樣嘢重砌：打印機主圖同 `/app/` 嘅即場原型。底係淡淡嘅打印板格仔，大面積嘅卡用噴嘴式切角（一邊圓角長、一邊短），主圖上面嘅打印路徑會自己畫出嚟，打印機晶片就話你知噴嘴貼住目標。總覽會用視窗框顯示原型，旁邊有三種語言模式同搞笑程度滑桿，所以主圖之後第一樣就係示範。

- **全部功能**（`ui-md3/site/features.js`）：Windows 版每項功能一張卡，一共十六張，每張都有用主色畫嘅內嵌動畫、資訊圖示、喺應用入面嘅位置，同一個開返「文件」分頁文章嘅連結。頭六張喺總覽做預告。
- **文件**（`ui-md3/site/docs.js`）：`docs/features` 入面每篇功能文章同 GitHub wiki 每一頁，全部喺網站入面。`scripts/build-docs-data.mjs` 喺組合時打包（有網絡就淺層複製 wiki，冇就靜靜跳過），需要嗰陣先載入。可以由一件事或者一個範疇開始，用共用搜尋欄同旁邊嘅正規表達式產生器搜尋兩種語言，睇英文版或者廣東話版，用旁白嘅聲音同速度朗讀，複製連結（`?article=<id>`）或者跳去原始碼。Markdown 喺瀏覽器用會轉義嘅轉換器繪製：文章入面冇嘢執行到。
- **工具**（`ui-md3/site/tools.js`）：指令面板（網站任何位置 `Ctrl+Shift+F`，或者頁首嘅搜尋掣），列出分頁、以即時開關顯示嘅設定、功能、文章同打開咗嘅面板嘅標題，支援 Enter、上、下、Esc，仲會直接傳送到嗰個元素；Base64 工具箱處理文字同檔案（圖片同 PDF 即場預覽，檔案留喺瀏覽器，大過 8 MB 會拒絕，保留最近十次結果）；無障礙工具（文字大小、高對比、減少動態嘅跟系統／開／關、永遠顯示焦點框、加大按鈕、朗讀頁面），頁首嘅無障礙掣都開到；同埋「動畫同過場」分頁（出現時間、分頁之間淡入淡出／滑動／冇、主圖路徑、卡片懸停浮起、預覽減少動態）。
- **動態**（`ui-md3/site/motion.js`）：捲動時章節升上嚟，分頁用短短嘅淡入或者滑動進場，功能圖會郁。減少動態係另一個設計，唔係放慢：未揀就跟系統，所有章節一早喺度，分頁即刻切換，圖畫完整，主圖路徑定喺一種主色。自動化瀏覽器（版面閘門、擷圖）一次過睇晒。

三個新分頁令版面閘門升到 555 格（156 格首頁、396 格逐個分頁、3 格角落疊卡）。`tests/site-redesign.test.mjs` 會檢查原始碼，每項都有一個一定要失敗嘅壞副本。

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
