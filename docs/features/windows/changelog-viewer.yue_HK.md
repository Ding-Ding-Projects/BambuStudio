---
translation-of: changelog-viewer.md
source-sha256: 593444650d0713f2ecdef8bc402a346a463c0a8554db242db6e8f736eb74975d
review-status: agent-drafted
---

> 英文原文：[In-app changelog viewer ("What's new")](changelog-viewer.md)

# 應用內更新日誌查看器（「有咩新嘢」）

呢個桌面應用程式附帶一個更新日誌查看器，涵蓋 **每個** 呢個分支發佈過嘅版本，唔只係最新嘅。佢可以從 **幫助 ▸ 有咩新嘢 / 更新日誌…**、從「關於」對話框上嘅 **有咩新嘢** 按鈕，以及因為命令調色板索引每個啟用嘅選單命令，可以通過輸入「what's new」或「changelog」進 <kbd>Ctrl</kbd>+<kbd>F</kbd> 來到達。

呢篇文章描述應用程式表面。Pages 網站有佢自己嘅查看器涵蓋相同嘅事實；睇 [更新日誌查看器（網站）](../pages/changelog-viewer.md)。

源；[`src/slic3r/GUI/ChangelogDialog.cpp`](../../../src/slic3r/GUI/ChangelogDialog.cpp)
（對話框同埋日曆彈出視窗）、[`src/libslic3r/Changelog.cpp`](../../../src/libslic3r/Changelog.cpp)
（wx 自由模型：解析、類型化日期、過濾、匯出）。

## 行為

- **每個版本，最新優先。** 每個發佈卡片顯示發佈號、佢嘅點心代號中英文恰好係發佈命名嘅樣子、UTC 發佈日期、標籤、一個 *發佈頁面* 連結，同埋每行一個提交喺嗰個發佈同埋前一個發佈之間。冇啲嘢列出嘅發佈會話邊三個誠實原因適用（最舊發佈、與前代相同提交、冇記錄提交）而唔係填充。
- **每個條目連結佢嘅提交。** 該行顯示 9 字符短 SHA 喺等寬連結中，其提示文字帶 40 字符完整 SHA；激活佢打開 `https://github.com/Ding-Ding-Projects/BambuStudio/commit/<sha>` 喺預設瀏覽器。該連結嘅可訪問名稱係「Open commit *sha* on GitHub」。
- **類別徽章** 從提交主題嘅前導動詞機械性派生（`fix…` → Fixed、`add…` → Added、`remove…` → Removed、`document…`/`handoff…` → Documented、其他 Changed）由導出器使用，網站使用相同規則。
- **搜尋** 通過共享 `SearchField` 藥丸執行；預設純文字，`.*` 切換同埋 `tune` 按鈕打開完整 [regex 構建器](regex-builder.md)，帶有大小寫、整詞同埋多行標誌。查詢匹配條目文字、短 SHA，同埋發佈嘅版本、標籤同埋代號（標頭命中保持嗰個發佈嘅所有條目）。
- **日期過濾器。** *從* 同埋 *到* 字段接受類型化日期；ISO `YYYY-MM-DD` 總係，或地區嘅短順序（`DD/MM/YYYY` 或 `MM/DD/YYYY`，用 `/`、`-`、`.` 或空格分隔）帶四位數年份。不完整或不可能嘅條目（`2026-09`、`8/9/26`、`30/02/2026`）保持喺字段中，並且內聯報告（「From 日期仲未完整。預期 …」）；前一個限制保持應用，直到文字再次變成日期。空字段係開放限制。一個 *To* 喺 *From* 前報告為「no version can match」而唔係默默交換。
- **日曆選擇器。** 日曆按鈕喺日期行下打開一個錨定彈出視窗；前一個同埋下一個月、月份選擇、年份轉軸（1970–9999）、Monday 優先 7×6 網格，帶有今天列明同埋選定範圍填充、一個 *清除日期* 操作同埋 *完成*。兩次點擊選擇範圍（第二次點擊喺第一次前交換佢哋）。鍵盤；箭頭按日同埋周移動、<kbd>PgUp</kbd>/<kbd>PgDn</kbd> 更改月份、<kbd>Home</kbd> 跳到今天、<kbd>Enter</kbd> 或 <kbd>Space</kbd> 揀、<kbd>Esc</kbd> 關閉。每個改動將 ISO 日期寫入字段並實時重新過濾列表。彈出視窗喺錨點上方翻轉或向左滑動，當佢會離開顯示時。
- **預設；** *過去 30 天*（今天同埋前面嘅 29 天）、*今年*（1 月 1 日到今天）、*所有版本*（清除兩個限制）。
- **組合。** 搜尋同埋日期範圍組合；狀態行陳述「*N* 個版本同埋 *M* 個更改已顯示（*T* 個版本總計）」。空狀態陳述邊個過濾器排除咗所有嘢同埋點樣擴寬佢。
- **顯示全部。** 前 30 個匹配發佈立即渲染；一個 *顯示全部 N 個版本* 按鈕渲染其餘嘅。複製同埋匯出總係涵蓋整個過濾集合，唔只係渲染卡片。
- **複製** 將過濾視圖放在剪貼板上作為 Markdown。**匯出…** 通過標準儲存對話框寫 Markdown（`.md`）或純文字（`.txt`）。兩種格式開始帶標頭，陳述版本庫、匯出範圍（`2026-01-01 to 2026-09-08`、`from …`、`until …` 或 `all versions`）、活躍搜尋同埋標誌，同埋計數；每個條目帶佢嘅完整 SHA（Markdown 另外連結佢）。結果通過應用嘅非阻塞通知 snackbar 公告；另一個應用程式持有嘅剪貼板或不可寫檔案報告為相同嘅錯誤通知，永遠唔係模態。
- **語言模式。** 所有複製通過 `_L()` 然後英文、香港粵語同埋雙語模式。菜名、版本、日期同埋 SHA 永遠唔會本地化或重新造型。
- **佈局。** 對話框係一個可調整大小嘅 MD3 殼層（最小 720×500 DIP，以 880×640 打開，限制到顯示），所以佢適應 1000×600 最小視窗。長條目換行；200% 尺度下冇嘢被剪掉。

## 數據同埋配置

查看器讀取 `resources/changelog/changelog.json`（與應用程式嘅 `resources/` 樹一起安裝），執行時冇網絡訪問。該檔案係 **生成同埋提交** ：

```
node scripts/changelog/export-app-changelog.mjs            # refresh from the GitHub Releases API + git log
node scripts/changelog/export-app-changelog.mjs --offline  # rebuild from ui-md3/site/changelog.data.js
node scripts/changelog/export-app-changelog.mjs --check    # exit 1 when the committed JSON is stale
```

導出器重用 [`ui-md3/scripts/build-changelog.mjs`](../../../ui-md3/scripts/build-changelog.mjs)
嘅解析（發佈名解析、主體元數據、動詞分類、`git log` 介於標籤之間），所以應用程式同埋網站永遠無法不同意版本、日期或類別。要求；Node 18+、已認證嘅 `gh`（在線模式）同埋已取得發佈標籤嘅檢出。完整刷新耗時約兩分鐘，因為每個引用嘅 SHA 個別解析。

**每個 SHA 都被驗證。** 每個發佈提交同埋條目 SHA 用 `git rev-parse --verify <sha>^{commit}` 解析；未知、不明確或非提交 id 導致導出失敗。應用程式側嘅解析器另外拒絕任何 SHA 唔恰好係 40 個十六進位字符嘅條目，所以死亡提交連結無法運送。導出器仲拒絕寫一個列出少於提交數量嘅檔案（部分 API 頁面或缺失標籤）。

架構（`schema: 1`）；

| 字段 | 意義 |
| --- | --- |
| `repository`、`commitUrlTemplate` | 擁有者/名稱同埋 `https://github.com/<repo>/commit/{sha}` |
| `generated`、`source`、`categoryDerivation` | 出處，跨同一刷新保持穩定 |
| `releases[]` | 最新優先；`tag`、`version`、`ordinal`、`date`（UTC `YYYY-MM-DD`）、`published`（ISO 即時）、`codeName.en/.yue`、`qualifier`、`url`、`commit`、`prerelease`、`baseline`、`sameCommit`、`build`、`entries[]` |
| `entries[]` | `sha`（40 個十六進位）、`short`、`text`（提交主題）、`category` |

冇用戶面配置；查看器冇佢自己嘅持久狀態。

## 失敗模式

- **缺失或損壞嘅數據檔案。** 對話框以「The changelog could not be read」打開、解析器嘅確切原因（檔案路徑、JSON 錯誤或有缺陷嘅字段同埋發佈）、檔案重新安裝恢復嘅路徑，同埋公開發佈 URL。複製同埋匯出用表示原因嘅提示禁用。
- **空發佈列表** 報告為這樣帶刷新命令。
- **冇匹配** 命名負責嘅過濾器（搜尋、日期或兩者）。
- **無效類型化日期** 永遠唔會改變過濾器；文字保持，內聯行解釋預期格式。
- **剪貼板忙 / 檔案唔可寫** 產生錯誤通知；冇嘢改變。
- **過時數據** 由 `--check` 捕捉，CI 可以喺網站自己嘅新鮮度檢查旁邊執行。

## 安全考量

- 執行時係離線；查看器讀一個綑綁嘅 JSON 檔案同埋只通過系統預設瀏覽器帶從提交範本同埋驗證 40 十六進位 SHA 構建嘅 URL 打開連結。
- Regex 搜尋由共享有界 regex 工作者評估，帶相同截止期限同埋大小限制為每個其他搜尋欄；一個災難模式超時並匹配冇嘢。
- 匯出只寫到用戶喺儲存對話框中揀嘅路徑；冇嘢被發送任何地方。
- 導出器執行 `gh` 同埋 `git` 唯讀，並只寫 `resources/changelog/changelog.json`。

## 驗證

- `tests/changelog/changelog_tests.cpp`（Catch2，目標 `changelog_tests`）；JSON 解析包括預設值同埋格式錯誤文件拒絕、帶每個 SHA 40 十六進位同埋最新優先排序嘅提交 `changelog.json` 解析、類型化日期解析（每個地區順序嘅 ISO、DMY vs MDY、分隔符、閏日、部分輸入拒絕、兩位數年份、字母同埋不可能日期）、民用日期算術、預設範圍同埋內含限制、過濾組合（只日期、只文字、標頭命中、兩者、冇匹配）、Markdown/純文字匯出標頭、範圍陳述同埋完整 SHA。最後執行；8 個測試案例，1718 個斷言，全部通過。
- `node scripts/changelog/export-app-changelog.mjs --check` 證明提交數據與發佈發佈相匹配。
- 手動；打開幫助 ▸ 有咩新嘢、輸入 `2026-09` 進「From」（內聯錯誤、列表未改變）、僅用鍵盤揀範圍喺日曆中、切換 `.*` 同埋搜尋 `^Fix`、複製同埋粘貼進編輯器、匯出為 `.txt` 同埋確認標頭命名範圍。

## 建議文章

- [Pages 網站上嘅更新日誌查看器](../pages/changelog-viewer.md)；相同數據，已發佈。
- [Regex 構建器](regex-builder.md)；搜尋藥丸嘅 `.*` 切換同埋構建器彈出視窗。
- [命令調色板 (Ctrl+F)](command-palette.md)；按名稱到達呢個對話框。
- [來自呢個分支發佈嘅應用程式更新](app-updates.md)；點樣呢度列出嘅版本到達。
- [發佈啟動頁面](release-splash-art.md)；點心代號來自邊度。
