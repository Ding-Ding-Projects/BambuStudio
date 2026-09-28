---
translation-of: dim-sum-surprise.md
source-sha256: df975ce65331a67b5fa6ac1d55d2624909372ba6e47723db760482165c4cf72e
review-status: agent-drafted
---

> 英文原文：[Dim sum startup surprise](dim-sum-surprise.md)

# 點心啟動驚喜

喺十次啟動中一次、桌面應用問候一個返回用戶同埋一個細卡喺主視窗嘅右下角：一個香港點心菜餚嘅照片、佢嘅名稱喺英文同埋繁體中文、同埋一句圍繞佢。佢淡入後視窗已安定、絕唔帶焦點、同埋淡出再次。

呢個係降落位置嘅桌面對應物 [點心驚喜](../pages/dim-sum-surprise.md)；兩個共用一個目錄同埋卡設計但唔係一行代碼。桌面實現居住喺 [`src/slic3r/GUI/DimSumSurprise.cpp`](../../../src/slic3r/GUI/DimSumSurprise.cpp) 同埋佢嘅視窗無服務模型喺 [`src/slic3r/GUI/DimSumSurpriseModel.hpp`](../../../src/slic3r/GUI/DimSumSurpriseModel.hpp)。

> 一個來自構建製品卡嘅捕獲係待定：卡喺十次有資格啟動中出現一次、同埋只有一旦照片已被快取、所以視覺煙霧工具需要一個溫暖快取同埋一個強制抽吸、然後佢能攝影佢。直到那存在、佈局被描述下方而唔係顯示。

## 行為

| 方面 | 發生什麼 |
| --- | --- |
| 機會 | 精確 10% 每次啟動、來自一個新鮮 `std::random_device`、種子 `std::mt19937` 抽吸。冇計算或儲存使佢更或更少可能。 |
| 頻率 | 最多一次每個過程。一個 `LaunchGuard` 聲稱抽吸喺首次呼叫；任何稍後呼叫係一個無操作。 |
| 時序 | 抽吸發生喺 `GUI_App::post_init` 後啟動嚮導決定。如果佢打、一個一次性計時器等待 1.5 s 所以卡絕唔喺視窗可用之前出現、然後有資格係再次檢查正確喺顯示之前。 |
| 菜餚 | 挑選均勻喺目錄菜餚之間誰嘅照片已喺磁碟上。 |
| 卡 | Material Design 3 卡喺 `SurfaceContainerHigh` 同埋一個 `OutlineVariant` 髮線、16 dp 角、一個 96 dp 圓角照片、一個 `Primary`、色調徽章（「點心驚喜」）、菜餚名稱喺 `Head_14`、同埋線喺 `Body_12` 喺 `OnSurfaceVariant`。寬度 380 dp、高度來自包裹文字。 |
| 焦點 | 卡係一個 `wxPopupWindow`；佢絕唔激活同埋絕唔接收鍵盤焦點。任何有焦點保持佢嘅。 |
| 關閉 | 自動關閉後 8 s。點擊喺卡上任何地方關閉佢。Escape 關閉佢太、通過一個 `wxEVT_CHAR_HOOK` 喺主幀、同埋按鍵仍然會到達有焦點嘅控件。休息指標喺卡上暫停計時器。 |
| 跟隨視窗 | 重新位置當主幀移動或調整大小；當幀被最小化或關閉時關閉。 |
| 運動 | 淡入超過粗糙 130 ms 除非 Windows 報告客戶端區域動畫禁用（`SPI_GETCLIENTAREAANIMATION`）、喺該情況佢立刻出現。 |
| Alt 文字 | 卡嘅無障礙名稱同埋工具提示係目錄嘅 alt 文字對菜餚喺兩個語言（「溫暖茶樓木桌上嘅經典蝦餃相片同埋 Classic Har Gow · 蝦餃」）。 |

### 語言模式同埋有趣等級

- **英文**：徽章「點心驚喜」、名稱「經典蝦餃 · 蝦餃」、英文線。
- **粵語**：徽章 點心驚喜、名稱 蝦餃 · 經典蝦餃、粵語線。
- **雙語**：英文徽章同埋名稱、英文線、然後粵語線堆疊下方。

圍繞名稱嘅句子跟隨每個語言有趣等級（`funny_level_en` 同埋 `funny_level_yue` 喺應用配置、1 到 5、預設 3）。等級 1 同埋 2 讀「十次啟動中一次。今天佢係…」；等級 5 有車停喺你嘅桌子。菜餚自己嘅名稱係目錄嘅 `name.en` 同埋 `name.zhHant` 準確喺每個等級同埋喺每個模式；唯有句子改變。英文源字串通過 `_L()` 所以一個翻譯目錄可能推翻佢們；粵語線船喺佢們旁邊所以雙語卡絕唔依賴一個目錄條目。

### 當佢保持家

卡被跳過、同埋一個 `info` 記錄線命名原因、當任何呢啲保持：

- 首次執行：`firstguide/finish` 唔係 `true`、或 `dim_sum_prior_launch` 記號冇被寫由一個早期啟動（呢個啟動寫佢一次上線同埋已完成）；
- 一個檔或 URL 被傳遞喺命令列（用戶係中任務）；
- 配置嚮導執行呢次啟動；
- 一個錯誤對話框被提高期間啟動（`DimSumSurprise::mark_startup_error()`）；
- 任何模式對話框喺計時器激發時打開（更新提示、隱私通知、任何）；
- Windows 報告一個安靜狀態通過 `SHQueryUserNotificationState`：忙、演示模式、安靜時間或一個全屏應用；
- 主幀係隱藏或最小化；
- 目錄快取係空、或冇照片被快取但是、或快取照片唔解碼（一個照片失敗解碼係刪除所以下一次獲取取代佢）。

冇**冇佔位符影像同埋冇後備菜餚**：同埋冇嘢顯示、冇嘢被顯示。

## 設定

冇。冇設定禁用驚喜、改變佢嘅賠率或挑選菜餚、同埋冇可能被添加。唯有儲存值係 `dim_sum_prior_launch` 記號喺 `BambuStudio.conf`、存在所以一個首次啟動絕唔被打斷。冇早期構建呢個分支曾經推出過一個退出設定鍵、所以冇嘢遷移。

## 目錄同埋照片源

唯有源作為名稱、alt 文字同埋照片係公共 [Ding-Ding-Projects/dim-sum-photos](https://github.com/Ding-Ding-Projects/dim-sum-photos) 資料庫：

- **目錄**：`https://raw.githubusercontent.com/Ding-Ding-Projects/dim-sum-photos/main/catalog/index.json`、架構 `1.x`、大約八十兆位元組為 ~2900 菜餚。`name.en` 同埋 `name.zhHant` 係權威；`image.path` 給照片嘅檔案名稱；`image.alt.{en,yue}` alt 文字。
- **照片**：發佈資產只、喺 `https://github.com/Ding-Ding-Projects/dim-sum-photos/releases/download/<volume>/<file>`。已發佈音量係 `catalog-v1`（菜餚 1 到 995）、`catalog-v1-part-002`（996 到 1985）同埋 `catalog-v1-part-003`（1986 到 3070）。菜餚數字挑選首個音量試試；其他係後備喺一個 404。

冇嘢來自目錄被承諾到呢個資料庫。應用保持一個應用資料快取喺 `data_dir()/dim-sum/`：

```
dim-sum/
  catalog.json        compact record: schemaVersion, sourceUrl, revision (ETag of the
                      raw response), fetchedAt (UTC), and per dish id/en/zhHant/altEn/altYue/file
  photos/<file>.png   up to six cached photos, each written atomically via a .part file
```

快取係喺一個背景線程喺每個有資格啟動溫暖、是否或無抽吸打、所以首個驚喜用戶係該欠一個照片準備。工作者製造最多一個目錄 GET（只有當 `catalog.json` 係缺失）同埋下載照片直到六係喺磁碟、放棄後三個連續失敗。佢絕唔阻止 UI 線程同埋冇嘢等待佢。

## 失敗模式

| 情況 | 結果 |
| --- | --- |
| 離線、空快取 | 目錄 GET 失敗；一行 `info`；冇嘢顯示呢次啟動。 |
| 離線、溫暖快取 | 卡從磁碟顯示。 |
| 目錄回應超過 24 MB、或一個照片超過 8 MB | HTTP 幫助器中止傳輸；冇嘢被寫。 |
| 目錄解析到零有效菜餚 | 唔被快取、`warning` 記錄、冇嘢顯示。 |
| 資產 URL 404 喺期望音量 | 其他音量按順序被嘗試。 |
| 下載資產唔係一個 PNG（簽名檢查） | 丟棄、`warning` 記錄。 |
| 快取照片失敗解碼或係在 64 px 之下 | 刪除、`warning` 記錄、冇嘢顯示呢次啟動。 |
| 一個模式對話框喺 1.5 s 延遲期間打開 | 稍後有資格檢查跳過卡。 |
| 主幀關閉而卡係起來 | 卡係用佢關閉。 |

## 安全同埋隱私

- 恰好兩種出站請求、兩個 HTTPS GET、兩個到 GitHub：原始目錄同埋發佈資產。冇查詢字串、冇身份識別、冇標題超越 libcurl 發送、冇遠端計量。冇嘢關於用戶或啟動離開機器。
- 傳輸係有界大小（24 MB 同埋 8 MB）同埋時間（30 s）、執行關閉 UI 線程、同埋被防守解析：一個記錄被丟棄除非佢嘅 id 匹配 `hk-dish-NNNN`、兩個名稱係呈現、同埋照片檔案名稱匹配 `hk-dish-NNNN-<slug>.png` 同埋一個小寫 slug。檔案名稱係唯有路徑成分應用組成、所以一個敵意目錄無法逃脫照片目錄。
- 檔被寫喺應用自己數據目錄通過一個暫時 `.part` 同埋重命名。
- 卡渲染一個解碼 `wxImage`、唔係瀏覽器檢視；目錄嘅描述同埋提示絕唔被渲染。

## 驗證

- `tests/dim_sum/dim_sum_tests_main.cpp`（Catch2、目標 `dim_sum_tests`）：抽吸分數超過 200000 種子試驗內 9、11%、每個過程一次守衛、完整有資格矩陣、目錄解析同埋有效、不完整、路徑遍歷同埋畸形記錄、影像檔案驗證、資產 URL 構造橫越所有三個音量同埋過去佢們、快取記錄往返同埋一個篡改條目、快取專用挑選同埋有趣等級副本絕唔改變菜餚名稱。設定 `DIM_SUM_TEST_CATALOG` 到一個下載 `index.json` 也解析實時目錄（隱藏測試標籤 `[.optional]`）。
- `src/slic3r/GUI/DimSumSurprise.cpp` 同埋 `GUI_App.cpp` 編譯 clean 喺 `cl /Zs` 同埋 GUI 目標嘅包括設定。
- 手動：設定 `funny_level_en` 同埋 `funny_level_yue` 同埋語言模式、溫暖快取通過啟動一次線上、然後啟動反覆；粗糙一個啟動喺十個顯示卡、絕唔兩個卡喺一個啟動、絕唔喺啟動帶一個檔案論證。

## 建議文章

- [發佈濺藝術](release-splash-art.md) 、 每發佈點心 SVG、一個單獨種子圖書館絕唔觸及公共目錄。
- [發佈代號稱](../releases/release-codenames.md) 、 發佈借一個菜餚名稱如何。
- [英文、香港粵語同埋雙語模式](language-modes.md) 、 卡尊重的語言模式。
- [原生Material Design 3 用戶介面](md3-native-ui.md) 、 令牌同埋排版卡被繪製。
- [點心驚喜喺降落位置](../pages/dim-sum-surprise.md) 、 位置嘅同一樂趣版本。
