---
translation-of: layout-probe.md
source-sha256: 54a41d3c89e4afb85384c1df21ed359b68ab622cccc5657f11332b2bd1e73b59
review-status: agent-drafted
---

> 英文原文：[Runtime layout probe](layout-probe.md)

# 執行時佈局探測器

一個預設關閉嘅測量模式，機械地尋找佈局剪裁，而唔係靠眼睛。

## 佢點解存在

一個訂閱過度嘅 `wxBoxSizer` 行唔會溢出。佢會按比例 0 項目全額支付，並將其後嘅項目分配成零寬度，所以挨餓嘅控制就簡直消失咗，雖然每個子項仍然報告一個喺其父項裏面嘅矩形。兩個主要控制（打印藥丸同埋流程標題）曾經係咁樣被丟失嘅，兩個都係靠睇螢幕截圖先至被發現。探測器會對每行嘅最小尺寸進行求和，對照佢嘅分配，並說出嚟。

## 行為

當 `BAMBU_LAYOUT_PROBE` 喺啟動時被設定，應用程式會寫一份每次轉儲嘅 NDJSON 檔案：

- 一份 `header` 記錄：原因、自由形式 `tag`（來自 `BAMBU_LAYOUT_PROBE_TAG`）、pid、DPI 比例、語言、深色模式、密度、頂級視窗數量；
- 每個頂級視窗一份 `toplevel` 記錄；
- 樹中每個視窗一份 `window` 記錄：類別（wx 類別名稱，所以 `Label`、`Button`、`TextArea` 而唔係 Win32 類別）、名稱、標籤、矩形、螢幕矩形、客戶端、最小值、最佳值、已展示、已啟用、父項句柄、擁有佢嘅 sizer 項目（比例、標誌、邊界、`CalcMin`、分配）同埋擁有嘅盒子 sizer 嘅判決（方向、可用、必需、訂閱過度）。

每份視窗記錄上嘅標誌：

| 標誌 | 意思 |
| --- | --- |
| `starved` | 一個已展示嘅 sizer 子項分配比佢自己嘅最小值要少 |
| `zero_sized` | 一個已展示嘅、寬度或高度為零嘅視窗 |
| `oversubscribed`（喺 `sizer.row` 裏面） | 盒子 sizer 嘅子項需要超過佢有嘅 |
| `text_clipped` | 一個標籤嘅文字範圍比佢嘅客戶端寬度要寬，同埋佢冇省略號樣式 |
| `ellipsized` | 標籤攜帶一個省略號樣式（為審查而報告，唔係發現） |
| `clipped_by_parent` | 一個已展示視窗嘅矩形會離開佢父項嘅客戶端區域 |

場景工具欄（`"toolbar":"main"`）嘅每個可見項目同埋 gizmo 欄（`"gizmo"`）一份 `gl_item` 記錄：名稱、主機 canvas 句柄、canvas 像素同埋螢幕上嘅矩形，源自項目嘅世界空間轉譯矩形同埋相機縮放。呢啲唔係 wx 視窗，所以冇標誌適用；佢哋存在係為咗一份截圖可以按名稱被裁剪到工具欄或欄項目。

每份帶標籤項目嘅 `wxAuiToolBar`（標題欄嘅品牌磚、選單工具、歷史記錄芯片、調色板同埋視窗控制）一份 `tool` 記錄，攜帶項目 id、佢嘅標籤或幫助文字，同埋佢喺工具欄同埋螢幕坐標中嘅矩形。工具唔係視窗，所以冇呢啲嘅話，標題欄對於重新截圖驅動程式就不可尋址，對於任何基於標籤嘅檢查亦係不可見嘅。

每份轉儲都以 `{"kind":"end"}` 結尾。一份輪詢檔案嘅讀者，而應用程式仍然流入佢，必須等待該記錄：一份由整行組成嘅部分檔案係幹淨地解析，並悄悄缺乏任何仲未被走過嘅嘢（第一次重新截圖就係咁樣丟失選項卡條嘅）。

命令通道（`WM_COPYDATA`、`dwData` 2）亦接受兩個驅動程式鉤子，而探測器係已武裝：`menu-popup <Title>` 會彈出標題欄嘅一個選單（檔案、編輯、檢視、物件、校正、幫助）同埋 `invoke <label>` 會觸發第一個標籤包含文字嘅選單項。兩個都經由 `CallAfter` 推遲，所以發送方嘅 `SendMessage` 會喺一個彈出迴圈或一個模式對話框阻止之前返回。按鍵同埋選單點擊唔會到達另一個桌面上嘅視窗；呢啲鉤子係無頭驅動程式如何到達選單後面任何嘢嘅方式。

## 啟動

| 設定 | 效果 |
| --- | --- |
| `BAMBU_LAYOUT_PROBE=1` | 寫 `<data_dir>/log/layout-probe-<pid>-<n>.jsonl` |
| `BAMBU_LAYOUT_PROBE=<dir>` | 寫入該目錄 |
| `BAMBU_LAYOUT_PROBE_TAG=<text>` | 複製進頭（命名元組：比例、語言、主題） |

一次轉儲會喺主框架首次展示同埋空閒後執行一次，每當流程收到 `WM_COPYDATA`（`dwData == 2` 同埋負載 `L"layout-probe [<path>]"`）時再次執行，呢個係無頭驅動程式喺打開對話框後要求轉儲嘅方式。未設定，成本係一個環境讀取。

`scripts/md3/send-layout-probe.py <hwnd> <out.jsonl>` 從標準庫單獨發送該訊息，給予主視窗句柄一份無頭視窗列表報告，並喺超時內冇轉儲出現時以非零碼退出。

## 讀取一份轉儲

```bash
node ui-md3/tests/layout-probe-report.mjs <dump.jsonl> [more.jsonl] [--json]
```

按嚴重性順序打印一份發現表格，並喺有任何發現時以非零碼退出，所以一份捕獲執行可以以佢為門禁。隱藏視窗同埋隱藏頂級視窗永遠唔計。

## 安全同埋隱私

轉儲包含視窗標籤，係用家可見字串，同埋視窗幾何。佢永遠唔包含檔案內容、憑據或項目數據，超過一個標籤展示嘅以外。佢只有喺環境變數被設定時先至被寫入。

## 驗證

- 來源契約：`ui-md3/tests/md3-conversion-contracts.test.mjs` 斷言門禁、`WM_COPYDATA` 調度、主框架展示後嘅安裝呼叫、CMake 登記同埋每份標誌名稱。
- 執行時：元組矩陣（100 / 125 / 150 / 200 百分比、英文 / 粵語 / 雙語、淺色 / 深色、舒適 / 緊湊）係喺構建嘅構件上執行，佢嘅發現、修復同埋前 / 後截圖被記錄喺 `docs/features/design-system/clipping-inventory.md`。

## 建議嘅文章

- [喺每個元素掃過中添加嘅套件組件](kit-widgets-2026-09.md)
- [MD3 奇偶校驗登記簿](md3-parity-register.md)
- [流程設定側邊欄](../prepare/process-settings-sidebar.md)
