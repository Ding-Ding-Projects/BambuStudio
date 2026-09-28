---
translation-of: generated-visual-showcase.md
source-sha256: dd04cdb745aa7644eac12f85740067a2dbeeb6be9d793116b74d262fcc0ebdfd
review-status: agent-drafted
---

> 英文原文：[Generated visual showcase](generated-visual-showcase.md)

# 生成視覺展示

互動 Material Design 3 參考同佢嘅 GitHub Pages 著陸頁面分享一個目的構建視覺套件喺 `ui-md3/assets/showcase/`。套件將切片器概念轉向編輯 3D 藝術品，同時保持工作介面、本地化同無障礙係獨立於圖像。

## 已發佈資産

| 資産 | 介面 | 主題 |
| --- | --- | --- |
| `hero-studio.webp` | Pages 英雄同應用程式首頁歡迎面板 | 桌面打印機、多面體打印同切片刀具路徑 |
| `home.webp` | Pages 功能卡 | 完成打印同可恢復項目工作空間 |
| `prepare.webp` | Pages 功能卡同最近項目 | 構建板排列同變換導引 |
| `preview.webp` | Pages 功能卡同最近項目 | 彩色 G 碼刀具路徑層 |
| `device.webp` | Pages 功能卡同最近項目 | 封閉打印機監視 |
| `multi-device.webp` | Pages 功能卡 | 協調四打印機車間 |
| `project.webp` | Pages 功能卡 | 模型圖片、零件同組件文件 |
| `filament.webp` | Pages 功能卡 | 墨水同物料圖書館 |
| `calibration.webp` | Pages 功能卡同最近項目 | 校準零件同尺寸檢查 |
| `settings.webp` | Pages 功能卡 | 亮/暗介面、密度同色調調色盤 |
| `og-social.webp` | Open Graph 同 X/Twitter 預覽 | 帶著著陸頁面標題嘅品牌社交卡 |

光柵源被生成作為原始藝術品、評論主題符合度同無不想要嘅文字，同編碼作為 WebP。完整已部署集係低於 700 KiB。生成源 PNG 冇被提交，因為佢哋新增冇執行時值。

## 行為同無障礙

- 著陸英雄迴圈加載，因為佢係最大優先檢視埠視覺。功能圖像非同步解碼同懶惰加載。
- 功能圖像有簡潔英文替代文字。最近項目縮圖同首頁歡迎藝術係裝飾，因為相鄰可見副本已經命名佢哋嘅操作。
- 文字、按鈕同本地化保持 HTML。冇本質指令被烤進圖像。
- 首頁圖像使用主題感知遮罩，所以英文、香港粵語同雙語副本保持對比。窄版面配置減少藝術品不透明度。
- 運動係禁用，當 `prefers-reduced-motion` 係活躍時。

## 設定

著陸頁面參考藝術品，從 `./assets/showcase/`。Pages 工作流程複製該資料夾到網站根，同完整應用程式複製保持佢喺 `/app/assets/showcase/`。版面配置斷言檢查著陸圖像路徑同社交卡，喺部署前。

## 失敗模式

- 如果資産被重命名，而唔係更新 `landing.html` 或首頁範本，Pages 版面配置測試根著陸參考失敗；應用程式模組化組件檢查捕捉範本漂移。
- 社交爬蟲需要一個絕對圖像 URL，所以 Open Graph 中繼資料使用規範 `ding-ding-projects.github.io` 位址。
- 圖像係增強唯一。如果圖像無法加載，所有導航、操作同描述保持可用。

## 驗證

從儲存庫根：

```powershell
node ui-md3/scripts/assemble-index.mjs --check
node --test ui-md3/tests/i18n.test.mjs
```

要喺本地複製 Pages 檔案版面配置，如 Pages 工作流程一樣組合 `_site` 同執行：

```powershell
node ui-md3/tests/assert-pages-layout.mjs _site
```

冇 Postman 人工製品適用；呢係一個靜態視覺介面，冇 HTTP API。
