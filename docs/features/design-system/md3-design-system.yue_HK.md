---
translation-of: md3-design-system.md
source-sha256: 11a549b7fb27e84730eef706d560058c9758e984d5a74152dbe00ea1d03f7b95
review-status: agent-drafted
---

> 英文原文：[Vendored Material Design 3 design system](md3-design-system.md)

# 內建 Material Design 3 設計系統

## 行為

原住民應用程式從一個單一內建 Material Design 3 工具包抽取佢嘅主題顏色、排字同佈局指標。倉庫設計源係 [`ui-md3/design-system/`](../../../ui-md3/design-system/)；原住民真實來源係 `src/slic3r/GUI/Widgets/MD3Tokens.hpp`，其令牌值與工具包完全匹配。C++ 代碼透過 `StateColor::semantic(MD3::Role[, ColorScheme])`、`ThemeColor` 同 `MD3::resolve(role, dark, scheme)` 而唔係硬編碼十六進制解析顏色。

`MD3Tokens.hpp` 被擴展為完整工具包平價（提交 `23688c23d`）。佢而家提供：

- 完整角色集合，包括 `OnError`、`OnErrorContainer` 同 `InversePrimary`、加上遮蔽同陰影色調；
- 一個仰角梯級 `elev1`–`elev5`（偏移 y 同模糊半徑、由主題陰影色調著色）；
- `MD3::Viewport` 軸同活躍顏色；
- 固定面板、對話同內容指標同形狀半徑喺 `MD3::Metrics` 底下；
- 完整 11 階 `MD3::Type` 比例（`headline` 透 `micro`）附帶 `Roboto` 同
  `Roboto Mono` 家族常數同 `Material Symbols Outlined` 圖示字體名稱；同
- `accentFromSeed()`、種子坡道連接埠重新生成六個口音角色從種子顏色。

一個從頭開始遷移然後轉換硬編碼主題顏色同字體橫越本質整個 GUI 樹──粗略 120 個檔案橫越六波：共用 Widgets 庫同 ImGui 主題；鉻同狀態欄；準備/Plater；預覽渲染器同時間表；三向量同視埠覆蓋層；設備、狀態面板、AMS、DeviceTab 同多機器表面；設定、參數同搜尋；葉片對話包括校準；剩餘檔案；同項目網頁視圖 CSS（令牌化）、附帶首頁網頁視圖驗證令牌化。數字同技術值使用新 `Label::Mono_*` 面由 Roboto Mono 支援。

情境計劃只交換口音角色按工作區同由活躍工作區解析：品牌綠色用於準備同一般 UI、預覽紫色用於 G-code預覽同設備青色用於打印機表面。

功能數據顏色係刻意保留同冇被遷移：墨水樣本、G-code功能顏色同 3D 繪製調色板保持佢哋數據承載意義。

## 配置

- 現有 Bambu Studio 外觀設定選擇淺或深色模式。主題變更更新全球 `StateColor` 模式在語意顏色被解析之前，然後重繪原住民小部件樹。
- 設定口音顏色透 `accentFromSeed()` 透過活躍工作區重新生成口音坡道。
- 情境計劃選擇由活躍工作區驅動、唔係由用戶設定。
- Roboto（常規、中等、粗體）同 Roboto Mono（常規、中等、粗體）運送喺
  `resources/fonts` 底下同由 `Label::initSysFont` 喺啟動時私下註冊；佢哋唔修改用戶嘅系統字體集合。

## 故障模式

- 一個遺漏嘅語意深色地圖項目回滾到佢嘅淺色令牌而唔係終止應用程式。
- 遺漏或不可用首選字體回滾透過現有系統字體路徑；CJK 地區使用佢哋內建家族因為 Roboto 唔包含嗰啲字形。
- 功能數據顏色係刻意豁免出令牌層；改變佢哋會改變意義，所以佢哋被保留未觸及。
- `Material Symbols Outlined` 圖示家族被命名為一個令牌但圖示字體載入基礎設施仲然唔係就位，所以圖示字形繼續使用現有位圖資產。

## 驗證

- 遷移樹嘅本地發布構建成功（依賴同應用程式；VS2022
  BuildTools、Windows SDK 10.0.26100）。
- 託管「Windows 構建同發布」工作流構建工作（`Build BambuStudio`）成功喺遷移樹上：執行 `29848731027`（頭 `7a027fa26`）同晚啲執行 `29862992010`
  （頭 `c700c91b0`、遠端 `master`）。發布工作係一個獨立關注；見
  [`../../../HANDOFF.md`](../../../HANDOFF.md)。
- 一個元素對元素對比審計針對設計工具產生一個矩陣。顏色、令牌同排字層報告完整：最後掃描發現 21 個剩餘主題字面、其中 18 個被處理喺嗰個工作中同 3 個被保留刻意超固定位圖資產（錨定同合理化喺下方「保留主題字面」）。剩餘三角洲係結構化組件解剖、唔係色錯誤：相機 HUD 覆蓋層系統、Material Symbols 圖示字體基礎設施同某啲藥丸幾何變數。呢啲被追蹤為未來工作喺
  [`../../../ROADMAP.md`](../../../ROADMAP.md)。
- 藥丸幾何喺共用 Widgets 庫係完整：每個控制工具包呼叫藥丸衍生佢嘅角落半徑從高度 / 2 喺繪製或佈局時間。`Button::applyMD3Style()` 設定
  `SetCornerRadius(FromDIP(height) / 2.0)` 同喺 `Rescale()` 上重新執行、所以半徑存活 DPI/密度變更；`SwitchButton` 用 `size.y / 2` 繪製佢嘅追蹤同拇指每繪製。規則而家被命名累加為 `MD3::Metrics::pill_radius(height)`。分段控制
  （`SwitchBoard`、`MultiSwitchButton`）係刻意唔係藥丸──工具包用固定追蹤半徑繪製佢哋。剩餘藥丸幾何變數係功能級控制冇專用小部件類別（過濾/選擇晶片、搜尋欄位藥丸、設定導航項目藥丸）同屬於鉻/設定表面。

### 保留主題字面

下方每位點保持一個原始顏色字面有目的因為固定位圖資產烤顏色一個主題角色會否則戰鬥。每個係主題獨立設計、所以令牌化佢到一個角色喺淺同深色計劃間翻轉會反轉佢個人對比同在一個模式中破壞元素。審計嘅三個保留超位圖字面係：

- **組件樹刪除徽章**──`src/slic3r/GUI/Overview/AssemblyStepsUtilsImgui.cpp:4646-4647`。
  `badge_bg = IM_COL32(77, 77, 77, 255)` 支援一個淺十字由著色固定
  `cross_dark.svg` 資產（`m_tree_icon_cross_dark`、載入喺列 3901），烤一個淺
  `#E0E0E0` 字形所以佢讀喺永遠深徽章（Figma 4098:10802/10803）。`badge_cross =
  IM_COL32(255, 255, 255, 255)` 係嗰個著色。徽章係一個主題獨立覆蓋層：一個中性角色會喺深色模式翻轉支援淺同隱藏淺十字，所以兩個字面保持綁定到資產。（兄弟關閉 X 喺列 5295-5300 可以著色同一個紋理從 `OnSurface`/`Outline` 只因為佢嘅背景係一個真實主題表面、唔係固定徽章。）
- **Helio 評級標題橫幅**──`src/slic3r/GUI/HelioReleaseNote.cpp:3168-3169`。
  `header_bg = wxColour(16, 16, 16)`（附帶佢嘅伴隨 `header_text = wxColour("#FEFEFF")`）包含固定 `helio_icon` 品牌位圖同、按代碼評論、「永遠為 Helio 品牌保持深」喺兩個主題。冇相反角色喺兩個模式保持淺、所以近黑橫幅同白色文字保持符合伴隨品牌資產而唔係令牌化到翻轉表面角色。

兩個進一步字面保持原因唔係位圖資產案例；佢哋唔係部分審計嘅「3 超固定位圖資產」計數同被追蹤為令牌對比後續：

- **預覽時間表目前步驟標記**──`AssemblyStepsUtilsImgui.cpp:823`（白旋鈕
  `IM_COL32(255, 255, 255, 255)`）同 `:835`（近黑數字 `IM_COL32(0x32, 0x3A, 0x3D, 255)`）。
  呢啲係一個配對固定標記取自 Figma 規格（白藥丸加深色數字）；令牌化任何一半單獨會反轉另一半嘅對比、所以對被保留主題獨立。
- **「未儲存視圖」琥珀點**──`AssemblyStepsUtilsImgui.cpp:4606`、`IM_COL32(0xFF, 0xA0, 0x00, 255)`。`MD3Tokens.hpp` 冇琥珀/警告角色為呢個狀態顏色解析到（一個令牌對比間隙）、所以字面保持直到嗰啲角色存在。
- 鮮新完整合成器擷取完整令牌遷移原住民表面仲然待批；而家已安裝應用程式擷取預期全掃。
