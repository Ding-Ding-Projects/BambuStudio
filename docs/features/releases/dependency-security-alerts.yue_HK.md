---
translation-of: dependency-security-alerts.md
source-sha256: e103387d85e358d9401f4e8ff01dd5f3ddc639142974cf7016886edc74efb9c0
review-status: agent-drafted
---

> 英文原文：[Dependency security alerts](dependency-security-alerts.md)

# 依賴套件安全警報

GitHub Dependabot 會將安全公告同呢個 repository 追蹤緊嘅套件 manifest 逐個對。呢篇文章記低有邊啲
manifest、邊啲會變成用家收到嘅嘢、一個警報點樣分流，同埋到而家為止每個警報點樣決定。第一次完整分流嘅
追蹤 issue 係 [#47](https://github.com/Ding-Ding-Projects/BambuStudio/issues/47)。

## 掃描咩，同埋每個 manifest 餵俾邊個

| Manifest | 套件管理器 | 邊個用佢 | 會唔會去到用家手 |
|---|---|---|---|
| `src/slic3r/GUI/DeviceWeb/device_page/package.json` 同 `pnpm-lock.yaml` | pnpm 10.12.1 | CMake target `device_page_build` 用佢自己指定嘅 Node 22.22.2 同 pnpm 行 `CI=1 pnpm install` 同 `pnpm run build`，然後將 `dist/` 抄去 `resources/web/device_page/dist`，Windows app 就係載入呢度。 | 只有頁面 import 嘅套件先會入 bundle（見下面）。 |
| `src/slic3r/GUI/DeviceWeb/device_page/package-lock.json` | npm | 冇人用。CMake、每個 workflow、每個 script 同頁面嘅 README 全部用 pnpm。上游 Bambu Studio 有呢個檔，所以留住佢，免得每次 merge 上游都撞衝突。 | 唔會 |
| `tests/web-e2e/package.json` 同 `pnpm-lock.yaml` | pnpm | 一套 Playwright 端對端測試，要人手對住 app 行。冇任何 workflow 會行佢。 | 唔會 |
| `resources/web/guide/swiper/…/package.json`、`resources/web/include/swiper/…/package.json` | 冇 | 隨附嘅 Swiper build 嘅 metadata；冇 lockfile，亦冇嘢會由佢度安裝。 | 隨附嘅檔案原封不動咁出貨。 |
| `ui-md3/desktop/package.json` | 冇 | 冇 lockfile；GitHub Pages 網站係靜態嘅，冇安裝步驟。 | 唔會 |

`dist/` 冇被追蹤。Windows build and release workflow 每次 push 都會重新 build 個頁面；因為 `CI` 已經設定，
pnpm 唔肯改 lockfile：build 會一字不差咁安裝 `pnpm-lock.yaml` 寫嘅嘢，而一個同 `package.json`
（包括佢嘅 `pnpm.overrides`）唔一致嘅 lockfile 會令 build 失敗。

### 出貨嘅頁面入面有咩

頁面自己嘅原始碼 import `react`、`react-dom`、`react-i18next`、`i18next`、`zustand`、`@radix-ui/*`、
`radix-ui` 同 `@tanstack/react-router`。2026-09-29 build 出嚟嘅 `assets/index.js.map` 列出 238 個來源：
128 個嚟自頁面嘅 `src/` 同 `locales/`，110 個 module 嚟自 36 個套件。嗰 36 個係 Radix UI 嘅基本元件、
`@tanstack/history`、`@tanstack/react-router`、`@tanstack/react-store`、`@tanstack/router-core`、
`@tanstack/store`、`aria-hidden`、`get-nonce`、`i18next`、`immer`、`react`、`react-dom`、
`react-i18next`、`react-remove-scroll`、`react-remove-scroll-bar`、`react-style-singleton`、
`scheduler`、`tslib`、`use-callback-ref`、`use-sidecar`、`use-sync-external-store` 同 `zustand`。
`vite`、`postcss`、`nanoid`、`eslint`、`vitest`、`undici`、`js-yaml` 呢啲 build 同開發工具從來唔會出現喺入面。

## Dependency graph 已經停用

2026-09-29 呢個 repository 嘅 Insights 頁面顯示「Dependency graph is disabled」，SBOM 匯出
（`gh api repos/Ding-Ding-Projects/BambuStudio/dependency-graph/sbom`）回應 404。repository 冇掛任何
組織 code security 設定。實際上即係：

- **push 之後 Dependabot 冇重新掃描 lockfile。** 2026-09-26 merge 上游（`22151a379`）帶嚟修正版之後，
  #10 同 #17 仲開咗三日；`75fc64c69` 升咗鎖版本之後，#2 同 #19 亦冇辦法自動變做 fixed。
- **警報清單短或者係空，唔代表 lockfile 乾淨。** 要直接檢查鎖定嘅版本，方法見下面。
- 2026-09-29 最遲到 07:27 UTC 仲有新警報開出嚟（#24），所以 graph 係最近先至停用或者停咗更新。要唔要
  重新開返，係 repository 設定，由維護者決定。

## 點樣分流一個警報

1. **搵出個 manifest 係俾邊個用。** 睇上面個表。喺 `package-lock.json` 嘅警報，講緊嘅係一個冇人會由佢度
   安裝嘅檔案。
2. **問：個套件會唔會出貨？** 用 CMake 嘅方法 build 個頁面，再喺 source map 列出啲套件：

   ```bash
   cd src/slic3r/GUI/DeviceWeb/device_page
   CI=1 pnpm install
   pnpm run build
   node -e 'const m=JSON.parse(require("fs").readFileSync("dist/assets/index.js.map","utf8"));const s=new Set();for(const p of m.sources){const r=/node_modules\/(?!\.pnpm)(@[^/]+\/[^/]+|[^/]+)\//.exec(p);if(r)s.add(r[1])}console.log([...s].sort().join("\n"))'
   ```

   做完之後刪走 `node_modules/` 同 `dist/`，兩個都喺 gitignore 入面。router plugin 可能會用唔同嘅換行
   重寫 `src/routeTree.gen.ts`（內容一樣），用 `git checkout` 還原就得。
3. **問：有漏洞嗰段 code 行唔行得到？** 喺 `node_modules/.pnpm` 搵出每個 import 呢個套件嘅地方，睇佢點樣
   call 公告講嗰個 function。一個只會讀 repository 自己檔案嘅 build、lint 或者測試工具，外人冇辦法餵惡意
   輸入俾佢。
4. **將鎖定版本同每一份公告對一次，包括已經 dismiss 咗嘅。** 有一條自動分流規則會喺開單一秒內 dismiss
   開發用套件嘅低影響警報（到而家有 #4、#5、#8、#9、#22、#23 同 #24），所以佢哋唔會出現喺未處理數字度。
   將每個 `pnpm-lock.yaml` 入面每個 `name@version`，同
   `gh api "repos/Ding-Ding-Projects/BambuStudio/dependabot/alerts?per_page=100"` 列出嘅所有警報（無論咩
   狀態）嘅漏洞範圍逐個對。下面個 `undici` 鎖版本就係咁樣搵到。
5. **更新會出貨或者行得到嘅，同埋每一個鎖住有洞版本嘅鎖。** 頁面 `package.json` 入面嘅
   `pnpm.overrides` 係保安用嘅鎖（`undici`、`js-yaml`、`@babel/core`、`brace-expansion`、`nanoid`）。
   鎖版本嘅作用係鎖住修正版，所以公告一包埋鎖住嗰個版本，就將個鎖移去第一個修正版，再用指定嘅 pnpm
   重新生成 lockfile：

   ```bash
   pnpm install --lockfile-only
   ```

   跟住檢查 frozen install 仲過唔過（`CI=1 pnpm install`）、頁面 build 唔 build 到，同埋比較前後嘅
   `dist/`。升一個 build 工具，啲檔案應該逐個 byte 一樣。
6. **其餘嘅喺警報度寫低原因再 dismiss。** 有漏洞嘅 code 喺度根本行唔到就用 `not_used`；`main` 上面嘅
   lockfile 已經冇咗報告嗰個版本就用 `inaccurate`。GitHub 限制 dismiss 留言最多 280 個字元，所以要連去
   追蹤 issue 睇完整證據。

## 2026-09-29 嘅分流

當時有 17 個未處理警報（9 個高、8 個中），涉及四個 manifest 入面嘅七個套件。冇一個被標記嘅套件會出貨，
亦冇一段有漏洞嘅 code 可以由 build 機以外嘅地方行到。十七個而家全部 dismiss 咗：13 個係 `not_used`，
4 個係 `inaccurate`，因為 `main` 上面嘅 lockfile 已經冇咗被標記嗰個版本，而 Dependabot 冇辦法重新掃描。

| 警報 | 套件 | Manifest | 鎖定版本 | 決定 |
|---|---|---|---|---|
| #2 | `nanoid`（GHSA-2v37-7h3g-55p8） | 裝置頁 `pnpm-lock.yaml` | 3.3.17，有鎖 | `75fc64c69` 將鎖升到 3.3.18；dismiss 做 `inaccurate`。 |
| #19 | `js-yaml`（GHSA-2883-xcg3-v3hh） | 裝置頁 `pnpm-lock.yaml` | 4.3.1，有鎖 | `75fc64c69` 將鎖升到 4.3.2；dismiss 做 `inaccurate`。 |
| #10 | `browserslist`（GHSA-73wf-gq98-2v4g） | 裝置頁 `pnpm-lock.yaml` | 4.28.8 | `22151a379` 之後已經係修正版；dismiss 做 `inaccurate`。 |
| #17 | `baseline-browser-mapping`（GHSA-w5vr-8v7q-w6rv） | 裝置頁 `pnpm-lock.yaml` | 2.11.26 | `22151a379` 之後已經係修正版；dismiss 做 `inaccurate`。 |
| #12、#14 | `@vitest/mocker`、`vitest`（GHSA-82fw-gwwq-j7x9） | 裝置頁 `pnpm-lock.yaml` | 3.2.7 | dismiss 做 `not_used`。 |
| #16 | `vitest`（GHSA-82fw-gwwq-j7x9） | 裝置頁 `package.json` | `^3.2.7` | dismiss 做 `not_used`。 |
| #1、#7 | `nanoid`（GHSA-2v37-7h3g-55p8、GHSA-xwg4-73v4-xw9w） | 裝置頁 `package-lock.json` | 3.3.11 | dismiss 做 `not_used`：冇人用嘅 lockfile。 |
| #3 | `brace-expansion`（GHSA-3jxr-9vmj-r5cp） | 裝置頁 `package-lock.json` | 2.1.0 同 1.1.14 | dismiss 做 `not_used`：冇人用嘅 lockfile。 |
| #6 | `@humanfs/node`（GHSA-p498-v437-472g） | 裝置頁 `package-lock.json` | 0.16.7 | dismiss 做 `not_used`：冇人用嘅 lockfile。 |
| #11 | `browserslist`（GHSA-73wf-gq98-2v4g） | 裝置頁 `package-lock.json` | 4.28.6 | dismiss 做 `not_used`：冇人用嘅 lockfile。 |
| #13、#15 | `@vitest/mocker`、`vitest`（GHSA-82fw-gwwq-j7x9） | 裝置頁 `package-lock.json` | 3.2.7 | dismiss 做 `not_used`：冇人用嘅 lockfile。 |
| #18 | `baseline-browser-mapping`（GHSA-w5vr-8v7q-w6rv） | 裝置頁 `package-lock.json` | 2.10.43 | dismiss 做 `not_used`：冇人用嘅 lockfile。 |
| #21 | `js-yaml`（GHSA-2883-xcg3-v3hh） | 裝置頁 `package-lock.json` | 4.3.0 | dismiss 做 `not_used`：冇人用嘅 lockfile。 |
| #20 | `js-yaml`（GHSA-2883-xcg3-v3hh） | `tests/web-e2e/pnpm-lock.yaml` | 4.3.0 | dismiss 做 `not_used`。 |

點解每個套件都行唔到，喺已安裝嘅原始碼度逐個查過：

- **`nanoid`**：唯一 import 佢嘅係 `vite` 入面嘅 `postcss`，build 嗰陣用 `nanoid/non-secure`，長度固定係 6。
  兩份公告要嘅係：將一個 call 嗰邊控制、大過或等於 2^31 嘅長度傳俾用 pool 嗰個 `nanoid(size)`，或者將長度
  0 傳俾 `customAlphabet` 或 `customRandom`。
- **`js-yaml`**：只有 `@eslint/eslintrc`（淨係讀舊式 `.eslintrc.yml` 先會 parse YAML）同 `i18next-parser`
  （淨係 `.yml` 目錄或者 YAML 設定先會 parse YAML）import 佢。頁面用嘅係 flat `eslint.config.js`、JSON 目錄
  同 JavaScript 嘅 parser 設定；`tests/web-e2e` 都係用 flat `eslint.config.js`。
- **`vitest` 同 `@vitest/mocker`**：公告要 Vite dev server 嘅 HMR socket 上面有公開嘅 `mockerPlugin` 或
  `interceptorPlugin`，或者用 Vitest browser mode。頁面只係用 `vi.mock` hoisting 行一個 jsdom 測試
  （`pnpm run test:a11y`），冇 `@vitest/browser`，兩個 plugin 都冇註冊。workflow 唔行測試。修正只喺
  vitest 4.1.11 先有。
- **`browserslist`、`baseline-browser-mapping`、`@humanfs/node`、`brace-expansion`**：pnpm lockfile 已經
  解析到修正版（4.28.8、2.11.26、0.16.8 同 5.0.9）；只有冇人用嘅 npm lockfile 仲寫住舊版本。

### 自動 dismiss 咗嘅警報，都一樣檢查過

- **#24**（`undici`，GHSA-3wwx-pv8p-q78v，7.29.1 修正）撞正頁面自己 `pnpm.overrides` 將 `undici` 鎖喺
  7.29.0。`9eb6ee5d2` 已經將個鎖升到 7.29.1。`undici` 只係開發工具用：本機測試嘅 jsdom，同 i18next-parser 入面嘅
  cheerio；vitest 測試就係用緊佢，全部過晒。
- **#22、#23**：同一份公告，喺冇人用嘅 npm lockfile 度（`undici` 7.28.0 同 8.9.0）。
- **#4、#5**（`brace-expansion`，GHSA-rgw5-rvv9-x895）同 **#8、#9**（`browserslist`，GHSA-c83g-rgw3-j3cx）：
  頁面嘅 pnpm lockfile 解析到嘅版本唔喺嗰啲範圍入面。

改完之後，頁面 `pnpm-lock.yaml` 入面仲跌落警報清單任何一份公告範圍嘅鎖定版本，就只有 `vitest` 同
`@vitest/mocker` 3.2.7。`tests/web-e2e/pnpm-lock.yaml` 入面仲有 `js-yaml` 4.3.0（警報 #20）同
`brace-expansion` 5.0.8（喺 GHSA-rgw5-rvv9-x895 範圍入面，但係嗰度從來冇開過警報）；兩個都係人手先行嘅
測試架嘅開發工具。

## 失敗情況同限制

- **lockfile 同 `pnpm.overrides` 唔一致，Windows build 會失敗。** 改完 override 一定要用 pnpm 10.12.1
  重新生成 `pnpm-lock.yaml`，push 之前證明 `CI=1 pnpm install` 過到。
- **dependency graph 停咗嘅時候，Dependabot 會追唔上 lockfile。** 處理 `pnpm-lock.yaml` 嘅警報之前，
  先睇清楚 lockfile 真正寫住邊個版本。
- **npm lockfile 唔會為保安而維護。** 佢嘅警報會 dismiss 做 `not_used`。如果有一日有 build 開始用 npm
  安裝，呢個決定就要重新諗過。
- **路徑太深會搞壞本機檢查。** 喺 Windows，pnpm 嘅 patch 步驟會轉入打咗 patch 嘅 `minimatch` 資料夾，
  Node 又會讀 `vite` 嘅 `package.json` imports；checkout 深過大約 180 個字元，兩個都會以
  `ENAMETOOLONG` 或者 `ERR_PACKAGE_IMPORT_NOT_DEFINED` 失敗。好似 build 咁，喺 repository 自己嘅
  `device_page` 資料夾入面做檢查就得。
- **有一個 DeviceWeb 測試因為無關嘅原因失敗過，直到 `0a0bb64c1`。** 由 `68f42a887` 開始，
  `tests/buildSpoolFromTray.test.ts` 會以 `ERR_MODULE_NOT_FOUND` 失敗，因為
  `src/features/filament-manager/constants.ts` import 冇副檔名嘅 `../../i18nResources`，用
  `--experimental-strip-types` 行測試嗰陣 Node 嘅 ESM loader 搵唔到。`0a0bb64c1` 加返 `.ts` 副檔名，之後 5 個
  測試全部過晒。
- **其他改動都可以令升鎖版本嘅 build 變紅。** `46792f02a` 嘅 run 36614198573 喺一個今次完全冇掂過嘅 C++ 檔
  （`AppearanceEditorPopover.cpp`，嚟自 `8c1e4a5ab`）失敗。怪 lockfile 之前先睇失敗嗰步：`device_page_build`
  嗰幾行會話你 pnpm 步驟過咗未。

## 安全考慮

新 lockfile 項目嘅 integrity 已經同 npm registry 公佈嘅數值對過。冇一個被標記嘅套件會去到 Windows app 或者
GitHub Pages 網站，所以冇任何已安裝嘅版本受影響。保持保安鎖版本最新，主要係為咗 build 機，同埋喺自己部機
行開發工具嘅人。

## 驗證

2026-09-29，用 CMake 指定嘅 Node 22.22.2 同 pnpm 10.12.1，喺未郁任何鎖版本之前、`75fc64c69` 之後，同埋
`undici` 鎖版本升咗之後：

- `CI=1 pnpm install`（frozen lockfile）次次都過。
- `pnpm run build` 次次都過，三次 build 出嚟嘅 `dist/` 16 個檔（4,079,543 bytes）逐個 byte 一樣。
- `pnpm run test:a11y` 次次都係 7 個過晒 7 個。
- 5 個唔使依賴套件嘅測試次次過咗 4 個；`tests/buildSpoolFromTray.test.ts` 次次都因為上面講嘅原因失敗，
  `0a0bb64c1` 之後就過咗。

C++ app 只會由 Windows build and release workflow build，嗰個 workflow 唔行測試。喺雲端 Windows runner 度，
run 36611172274（`75fc64c69`）同 run 36614198573（`46792f02a`）嘅 `device_page_build` 步驟都寫住
「Lockfile is up to date, resolution step is skipped」，用 pnpm 10.12.1 裝完再 build 好個頁面。

第一批帶住新鎖版本嘅 release 係 `md3-v169`（由 `087fe6f70` build，有 `js-yaml` 同 `nanoid` 兩個鎖）同
`md3-v170`（由 `784d86ff3` build，三個都有）；`md3-v171`（由 `85a1d9e86` build）一樣有齊三個。`md3-v171`
隨附嘅 CycloneDX 清單（`BambuStudioMD3.cdx.json`）列出 `resources/web/device_page/dist` 嘅 16 個檔，SHA-256
同未郁任何鎖版本之前嘅本機 build 一模一樣，即係用家安裝到嘅頁面完全冇變過。
