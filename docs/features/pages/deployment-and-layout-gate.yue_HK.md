---
translation-of: deployment-and-layout-gate.md
source-sha256: 376d92145d8d9b6b9c2e849b7d385a54f552abdfba6fe00f4d89f6245b98949e
review-status: agent-drafted
---

> 英文原文：[Deployment and the layout gate](deployment-and-layout-gate.md)

# 部署同佈局把關

工作流程：[`.github/workflows/ui-md3-pages.yml`](../../../.github/workflows/ui-md3-pages.yml)。
佢喺每個推送觸發 `ui-md3/**`、工作流程本身或靜態協議斷言嘅原生對話方塊時執行，同喺 `workflow_dispatch` 時執行。

## 組成網站

`node ui-md3/scripts/compose-site.mjs _site` 產生發佈樹：

```
_site/index.html      the tabbed landing site   (ui-md3/landing.html)
_site/site/           its modules and stylesheet (ui-md3/site/)
_site/assets/         bundled fonts and showcase artwork
_site/app/            the interactive prototype, served at /app/
_site/app/i18n*.js    the shared localisation runtime the landing loads
```

編輯器係一個已提交嘅腳本，而唔係工作流程中嘅內聯殼，所以 CI 發佈嘅樹可以字節對字節地喺開發機上重現，同佈局失敗可以本地調試，而唔係只喺執行器中。

`ui-md3/tests/assert-pages-layout.mjs` 然後檢查組成嘅樹：每個本地腳本、樣式表同圖像參考喺根內解析、網站模組都存在、完整展示同每個捆綁字體發運、冇第三方字體或腳本被引用、同網站模組喺執行時構建嘅藝術作品都解析咗，一個重命名嘅圖像會喺部署時失敗，而唔係 404-ing 實時。

第三方檢查掃描 **每** 發佈頁面，唔只係登陸頁面。佢喺 2026-07-28 之前被限制到根，這就係設計系統 UI 套件點 `/app/design-system/ui_kits/bambu-studio/` 點花費數月從 unpkg 加載 React、ReactDOM 同 `@babel/standalone` 嘅方式，喺一個網站，佢嘅第一個記錄承諾係佢冇做第三方請求。佢破裂嘅頁面根本唔係被檢查嘅頁面。協議相對 `//host/path` URL 都算作非主機；佢哋只係同源，出於偶然如何頁面被到達嘅。

## 組建生成頁面

樹中嘅兩頁係生成嘅，同喺網站被組成之前被檢查是否過時：

| 腳本 | 生成 | 來自 |
|:---|:---|:---|
| `assemble-index.mjs --check` | `ui-md3/index.html` | `ui-md3/app/screens/*.template.html` |
| `assemble-ui-kit.mjs --check` | UI 套件嘅 `index.html` | 佢旁邊嘅十二個 `.jsx` 源 |

UI 套件嘅組建器都編譯咗 JSX，使用 `ui-md3/scripts/jsx-transform.mjs`，一個無依賴嘅編譯器用於套件使用嘅子集，其輸出匹配 `@babel/plugin-transform-react-jsx` 嘅經典執行時，同對任何超出該子集嘅東西拋出，而唔係編譯佢到某物只係似乎合理嘅。呢嘅係移除咗 UI 套件用來喺每個頁面加載時執行嘅瀏覽器 Babel 變換。組建器拒絕構建一個頭部漂移回 CDN 嘅頁面，同拒絕一個兩個源聲明同一個頂級 `const` 嘅頁面，編譯嘅塊係共享全球作用域嘅經典腳本，所以碰撞係一個 `SyntaxError`，靜默殺死佢之後嘅每個腳本。

## 把關

喺任何嘢被上傳之前，工作流程執行：

| 套件 | 佢持有嘅 |
|:---|:---|
| `ui-md3/tests/i18n.test.mjs` | 共享本地化執行時同登陸嘅佢嘅使用 |
| `ui-md3/tests/site.test.mjs` | 複製梯、佔位符奇偶性、每級別事實、鍵覆蓋、正則表達式邊界、日期解析、變更日誌數據完整性、點心目錄 |
| `ui-md3/tests/layout-clipping.test.mjs` | 靜態協議：冇省略號、冇水平滾動器、44px 下限、帶狀溢出階段、tablist 語義 |
| `ui-md3/tests/site-behaviour.test.mjs` | 存儲往返、元素外觀應用/重置、通知記錄同持久性 |
| `ui-md3/tests/jsx-transform.test.mjs` | JSX 編譯器：同 Babel 輸出同空白規則嘅一致性，同拒絕超出佢嘅子集嘅一切 |
| `ui-md3/tests/offline-render.test.mjs` | 組成嘅網站渲染喺無頭瀏覽器中，帶 **每個非主機都被黑洞化** |
| `ui-md3/tests/runtime-layout-clipping.mjs` | **447 個測量網站個案（156 登陸 + 288 每標籤 + 3 緊湊角落表面個案）加上 6 個加載發佈原型嘅**，喺真實無頭瀏覽器中 |

`offline-render.test.mjs` 組成佢自己嘅網站副本、侍奉佢同用 `--host-resolver-rules=MAP * 0.0.0.0, EXCLUDE 127.0.0.1` 指向 Chrome，所以環回仍然連接同其他一切失敗如機器被拔掉。佢然後要求真實 UI 回來：掛載點有內容，而且最少有一定數量嘅元素同文字渲染出嚟。一個空白頁面同靜默依賴 CDN 嘅頁面兩個都失敗。靜態協議捕獲一個 **寫下** 嘅非主機 URL；呢個捕獲一個 **到達** 嘅，同佢係會更早捕獲 UI 套件幾年嘅檢查。

兩個套件都分享 `ui-md3/tests/devtools.mjs` 中嘅 Chrome 管道。

執行時工具係重要嘅。佢驅動 Chrome 通過 DevTools 協議對抗本地服務嘅組成樹副本：

- **156 登陸個案**，13 個物理寬度 × 4 顯示縮放 × 3 語言模式，斷言冇文件溢出、冇標題元素喺視口外、冇重疊標題控制、每個標題目標至少 44×44、冇特徵卡裁剪佢自己嘅內容、同語言模式實際被應用。
- **288 每標籤個案**，4 個寬度 × 3 縮放 × 3 語言模式 × 8 標籤，啟動頁面中嘅每個標籤同斷言面板渲染內容、冇元素喺佢內部定位喺視口外或有內容比佢自己寬、每個控制至少 44×44（一個 44px 標籤內嘅複選框計，一個內聯連結在散文內係文字，唔係目標），同標籤帶保持喺一行，而唔係包裹。

發佈喺 `/app/` 嘅原型都被測量，三個寬度喺兩個顯示縮放，斷言佢嘅視窗控制保持喺視口內、佢嘅標題欄唔溢出佢自己、同冇可見按鈕缺乏可訪問名稱。佢之前係未測量，這就係標題欄推動佢自己嘅關閉按鈕 217px 過去 640px 視口嘅方式到達生產。

因為工具喺 `scrollWidth > width` 時失敗，網站可能唔使用 `text-overflow: ellipsis` 或任何地方嘅水平滾動容器：嗰啲隱藏一個裁剪，而唔係修復佢。長字串包裹代替，這就係為什麼雙語模式喺 200% 縮放係一個第一級佈局個案，而唔係事後。

## 發佈

綠色執行上傳 `_site` 同用 `actions/deploy-pages` 部署佢。Pages 通過 `actions/configure-pages` 帶 `enablement: true` 自動啟用，所以新分叉上唔需要手動儲存庫設定。並發被限制為一個進行中部署，最新推送勝出。

## 本地重現把關

```bash
node ui-md3/scripts/compose-site.mjs _site
node ui-md3/tests/assert-pages-layout.mjs _site
node --test ui-md3/tests/i18n.test.mjs ui-md3/tests/site.test.mjs ui-md3/tests/layout-clipping.test.mjs
node ui-md3/tests/serve.mjs _site 4173 &
BAMBU_PAGES_TEST_URL=http://127.0.0.1:4173/index.html node --test ui-md3/tests/runtime-layout-clipping.mjs
```

執行時套件需要 Chrome 或 Edge；佢通過 `CHROME_PATH` 或常常安裝位置發現一個，同花費大約三分鐘用於所有 447 網站個案。
