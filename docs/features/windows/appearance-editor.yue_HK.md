---
translation-of: appearance-editor.md
source-sha256: c616f3455244c402454dd25aa24ab04d294ab6e77dc0f33c961f1fc060870da3
review-status: agent-drafted
---

> 英文原文：[Per-element appearance editor](appearance-editor.md)

# 逐個元素外觀編輯器

每一個已選擇加入嘅渲染元素都可以獨立重新設定樣式：右擊佢並選擇 **編輯外觀...**，當佢聚焦時按 **Ctrl+Shift+E**，或按 Shift+右擊跳過選單。一張非模態嘅 Material 卡片會喺個元素旁邊打開，跟蹤佢嘅移動，並將每一個變化寫入一個逐個元素嘅登記冊，讓實時 UI 立即重新讀取。

源文件：`src/slic3r/GUI/Appearance/ElementStyle.{hpp,cpp}`（登記冊、永久儲存、預設值、wxWindow 採用人）和 `src/slic3r/GUI/Appearance/AppearanceEditorPopover.{hpp,cpp}`（卡片、上下文選單幫手、快捷鍵）。

## 行為

### 打開路線

| 路線 | 喺邊度 | 結果 |
| --- | --- | --- |
| 上下文選單裏嘅 **編輯外觀...** | 每個已採用嘅 Material 選單（自動），加上物件列表、打印板、多選擇和墨水行選單（明確） | 卡片喺打開選單嘅元素旁邊打開 |
| **編輯分頁外觀...** | 項目分頁右擊選單（喺 *關閉分頁* 旁邊） | 卡片為該分頁打開 |
| Shift+右擊 | 任何已採用嘅元素、項目分頁 | 卡片直接打開，無選單 |
| **Ctrl+Shift+E** | 主框架裏聚焦嘅控制項、其中元素已採用嘅對話框內部 | 卡片為聚焦嘅控制項打開；未採用嘅控制項會喺 `focused/<name>` 下立即採用 |
| 指令面板 (Ctrl+F) | "編輯聚焦元素嘅外觀"、"套用外觀預設值：..." | 同快捷鍵／預設值開關 |

選單項目喺右對齐顯示其快捷鍵（`Ctrl+Shift+E`），如同每個其他 Material 選單行，快捷鍵列喺鍵盤快捷鍵對話框嘅 *全局快捷鍵* 下。

### 卡片

一個 `wxFrame` 工具視窗（無工作列項目、浮喺其父項上方、無原生框架），繪製為帶有 1 px OutlineVariant 邊框和軌道半徑嘅 SurfaceContainer 卡片。佢係 **非模態**：應用程式喺佢打開時仍可使用，元素會隨著數值變化而不斷重新渲染。

- **已錨定。** 放置位置重複使用 `MD3::Menu::place_root`（錨點下方，否則上方、右方、左方，然後縮小），因此視口邊緣碰撞就係同選單處理碰撞一樣；卡片永遠唔會重疊或脫離其錨點。120 毫秒嘅計時器會喺錨點嘅螢幕矩形改變時重新放置卡片，喺錨點滾動離屏時隱藏佢，喺錨點被銷毀時關閉佢。
- **一張卡片。** 為另一個元素打開會重新定位打開嘅卡片，而唔係堆疊第二張。
- **焦點。** Escape 或關閉按鈕關閉卡片並將焦點返回錨點。Ctrl+PageUp／Ctrl+PageDown 喺分節之間移動；Tab 遍歷每個控制項。
- **分節**（卡片頂部嘅分頁按鈕）：
  - *排版*：可搜尋嘅字型清單（來自 `wxFontEnumerator` 嘅已安裝字體加上捆綁嘅 Roboto、Roboto Mono、HarmonyOS Sans SC、NanumGothic、Source Han Sans JP 和 Symbola，標記為 *(捆綁)*)，喺所選字體中有一行實時預覽；大小（pt、步進器 + 自由項目）；粗細 100–900；斜體、下劃線、刪除線；字母間距；行高。大小、字母間距同行高係套件嘅小數欄（`AppearanceDecimalField`：套件 `TextInput` 加兩粒箭嘴步進掣）：上下方向鍵可以調整，打入嘅數字喺掹 Enter 或者離開欄位時生效，逗號當小數點，每個值都限喺範圍之內（4 至 96 pt，每步 0.5；-4 至 20 px，每步 0.1；0.8 至 3，每步 0.05）。
  - *顏色*：文字、背景、高亮、邊框。每個色板打開 Material 顏色選擇器（`MD3ColorPickerDialog`，包含其顏色轉譯器）；未設定嘅顏色顯示 *主題預設值* 並保持令牌。
  - *形狀與間距*：邊框寬度、角半徑、內補間距、外邊距（px）。
  - *預設值*：可搜尋嘅預設值清單（已出貨預設值標記、活躍嘅標記），**套用**、**另存為預設值...**、**刪除**（僅用戶預設值），**匯出主題...** / **匯入主題...** (JSON via `wxFileDialog`)。
- **重設** 喺三個範圍：每個控制項旁邊嘅逐個屬性撤銷按鈕（僅喺該屬性有用戶覆寫時啟用），頁腳中嘅 **重設元素**，和 **全部重設**（已確認；刪除所有覆寫並返回到 *Material 預設值* 預設值，保持已儲存嘅預設值）。
- 卡片 **遵循其自身嘅客製化**：其標題、標題、分節分頁和行標籤係已採用嘅元素（`appearance-editor.title`、`appearance-editor.caption`、`appearance-editor.tab`、`appearance-editor.label`），因此編輯器可以重新設定自身嘅樣式。
- 卡片中嘅每個搜尋欄都係共用嘅 `SearchField`，配有 `.*` 正則表達式切換和已錨定嘅正則表達式建構器；純文字保持為預設值。

### 解析模型

`(元素 id、屬性)` 嘅值通過三層解析，最具體優先：

1. 用戶對該元素嘅覆寫（檔案中嘅 `elements`）；
2. 活躍嘅預設值：其元素項目，然後其 `"*"` 項目；
3. 呼叫者嘅基本值（小工具會使用嘅 MD3 令牌）。

Id 可能喺斜線後帶一個父項：`project-tab/model` 喺每一層回退到 `project-tab`，因此一項規則可以設定每個分頁嘅樣式，而單個分頁仍然可以有所不同。預設值嘅 `"*"` 項目到達每個元素。

已出貨嘅預設值僅從 MD3 令牌衍生：*Material 預設值*（無覆寫）、*大文字*（15.5 pt、行高 1.4）、*緊湊文字*（12.5 pt、內補間距 6）、*圓形*（半徑 20 = `Metrics::radius_home`）、*粗體標籤*（粗細 600）。已出貨名稱永遠唔可以被覆寫或刪除，帶有已出貨名稱嘅檔案會被忽略該名稱。

### 連接嘅小工具（首次削減）

| 元素 id | 小工具 | 樣式達到嘅內容 |
| --- | --- | --- |
| `menu.item` | 每個 Material 選單行（`MD3Menu.cpp`） | 字型系列／大小／粗細／樣式／裝飾、文字顏色 |
| `project-tab`、`project-tab/<file stem>` | 項目分頁條（`ProjectTabBar.cpp`） | 字型、文字顏色、背景（分頁填充 + 懸停） |
| `preferences.row/<AppConfig key>`（父項 `preferences.row`） | 偏好設定中嘅每個開關行標籤 | 字型、文字顏色、背景 |
| `topbar` | 頂部條（`BBLTopbar`） | 條視窗嘅字型、文字顏色、背景；上下文選單 + 快捷鍵 |
| `object-list.row`、`object-list.plate`、`sidebar.filament-row` | 物件列表／打印板／墨水選單 | 僅選單項目，參考空白 |
| `appearance-editor.*` | 編輯器自身嘅 chrome | 字型、文字顏色、背景 |
| `focused/<name>` | 通過 Ctrl+Shift+E 到達嘅任何項目 | 字型、文字顏色、該控制項嘅背景 |

任何小工具都可以加入一個呼叫：`ElementStyle::apply(window, "id", display_name)` 記住視窗嘅目前字型／顏色作為基本顏色，套用已解析嘅樣式，喺每個登記冊變化時重新套用，喺 `wxEVT_DESTROY` 時釋放自身，並連接上下文選單、Shift+右擊和 Ctrl+Shift+E。自行繪製嘅小工具從令牌繪製會諮詢 `ElementStyle::font_for(id, base)`、`colour_for(id, role, base)` 和 `number_for(id, key, base)` 而不係喺佢們嘅繪製位置。

### 已知空白

- 僅上表中嘅小工具已連接。按鈕、側欄標籤、筆記本分頁欄和 3D 畫布 ImGui chrome 仍然從令牌繪製；佢們可以用 `ElementStyle::apply` 或繪製位置鉤子採用，但沒有樣式到達佢們。
- 物件列表、打印板和墨水行選單帶有項目，但佢們後面嘅 `wxDataViewCtrl` 行係原生嘅，尚未讀取登記冊：編輯這些 id 會記錄數值（編輯器在其自身行預覽佢們）而唔會改變列表嘅渲染。
- `letterSpacing`、`lineHeight`、`highlight`、`borderColor`、`borderWidth`、`radius`、`padding` 和 `margin` 係儲存和解析嘅，但只有測量或構築自身文字嘅小工具才能尊重佢們；原生 `wxStaticText` 忽略佢們。編輯器喺每個分節說明呢點。
- 錨點跟蹤係計時器輪詢（120 毫秒），而唔係移動事件訂閱，因此父視窗嘅快速拖動會顯示卡片追上嘅情況。

## 組態

存儲：`data_dir()/appearance/element-styles.json`，由 `AppearanceEditor::init` 喺 `GUI_App::on_init_inner`（喺套用已永久儲存嘅密度／強調色之後）加載，並通過編輯器（合併每個事件循環回合）或調色板預設值開關時每次變化時寫入。寫入會到一個 `.tmp` 檔案旁邊的檔案並重新命名為佢，因此崩潰中途寫入會保留先前的檔案。

架構（`"schema": 1`）：

```json
{
  "schema": 1,
  "activePreset": "Material default",
  "presets": { "<user preset>": { "*": { "fontSize": 15.5 }, "<element id>": { "foreground": "#146c2e" } } },
  "elements": { "<element id>": { "fontFamily": "Roboto Mono", "underline": true } }
}
```

已知屬性：`fontFamily`、`fontSize`（pt）、`fontWeight`（100–900）、`fontStyle`（`normal` | `italic`）、`underline`、`strikethrough`、`letterSpacing`（px）、`lineHeight`（乘數）、`foreground`、`background`、`highlight`、`borderColor`（`#rrggbb` 或 `#rrggbbaa`）、`borderWidth`、`radius`、`padding`、`margin`（px）。呢個係同網站嘅 `ui-md3/site/settings.js` 中嘅 `elementStyles` 模型同一形狀。

匯出寫入同一文件；匯入合併檔案嘅用戶預設值，採用其元素覆寫和活躍預設值，並保持其未知嘅頂級鍵。

## 故障模式

- **缺少檔案**：新登記冊；無任何項目被設定樣式。
- **無法解析 JSON／唔係物件／無整數 `schema`／架構比 1 新**：加載被拒絕，並附上原因（`StyleLoadReport::error`），登記冊保持其目前狀態，並記錄警告。檔案唔會被重寫，直到用戶變更某個項目。
- **未知嘅頂級鍵和未知嘅元素屬性**：通過加載、編輯和儲存逐字保留，喺 `StyleLoadReport` 中報告，並喺匯入後喺訊息中列出。無任何項目被無聲刪除。
- **未知嘅活躍預設值**：回退到 *Material 預設值* 並報告。
- **未知字型系列**：喺 `SetFaceName` 之前根據工作階段字型表驗證；未知字體會保留基本字體（wx 否則會使字體無效）。
- **超出範圍嘅數值**：大小超出 4–96 pt 會被忽略，粗細夾住到 100–900，無效色字符會保留基本顏色。
- **寫入故障**（唯讀目錄、磁碟滿）：記錄；內存中嘅登記冊保持套用，因此工作階段保持其外觀。
- **錨點喺卡片打開時被銷毀**：卡片喺下一個計時器上關閉，而唔會改變焦點。

## 安全考量

- 檔案係設定檔案目錄中嘅用戶資料；佢係用同 AppConfig 相同嘅信任度讀取。數值只會到達 `wxFont`、`wxColour` 和整數指標：無路徑、無指令、無 URL。
- 字型面喺 `wxFontEnumerator::IsValidFacename` 前被驗證以供使用，與 [外觀客製化](appearance-customization.md) 中嘅 GDI+ 私人字體規則一致。
- 匯入唯有讀取用戶選擇嘅 JSON 檔案；佢永遠唔會抓取任何東西，永遠唔會執行內容。
- 字型和預設值清單中嘅正則表達式搜尋通過已界定嘅正則表達式工作程式執行（參考 [正則表達式建構器](regex-builder.md)）。

## 驗證

- `tests/appearance/appearance_tests_main.cpp`（Catch2，目標 `appearance_tests`）涵蓋：屬性目錄；預設值無任何解析；設定／解析／逐個屬性／逐個元素／全局重設；通過 `/` 嘅父項繼承；預設值優先順序（用戶覆寫超過預設值項目超過 `"*"`）；另存為預設值快照；已出貨預設值被保護；JSON 往返保留和報告未知鍵（於重新解析時固定點）；拒絕垃圾、陣列、缺少和比 1 新嘅架構，而唔郁動狀態；未知活躍預設值回退；匯出 → 匯入相等和儲存 → 加載相等；字型／顏色幫手夾住；聽眾令牌。最後執行：14 個測試案例中 105 項聲明，全部通過（用相同配方反對 wx 核心／基礎手動連接，如 `tests/md3_menu`）。
- 每個變更嘅翻譯單位通過 `cl /Zs` 對項目嘅包含集通過（`MainFrame.cpp`、`GUI_App.cpp`、`GUI_Factories.cpp` 和帶有強制 PCH 包含嘅 `Preferences.cpp`）。
- 尚未驗證：建構應用程式嘅行駛執行（右擊 → 卡片 → 實時重新設定樣式）和卡片嘅捕獲；表面需要下一個完整建構。

## 建議文章

- [外觀客製化](appearance-customization.md)：此編輯器分層嘅全局主題、密度、強調色和字型控制。
- [Material 上下文選單](material-context-menus.md)：帶有 *編輯外觀...* 項目嘅選單。
- [Material 顏色選擇器與顏色轉譯器](md3-color-picker.md)：每個色板後面嘅選擇器。
- [正則表達式建構器](regex-builder.md)：字型和預設值搜尋欄後面嘅建構器。
- [指令面板](command-palette.md)：到編輯器和預設值切換嘅另一條路線。
