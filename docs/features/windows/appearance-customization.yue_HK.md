---
translation-of: appearance-customization.md
source-sha256: 6ea9a1aa830e22cab425a50385c0f829dc359a7b1d450bf4faad1d3974cfff57
review-status: agent-drafted
---

> 英文原文：[Appearance customization](appearance-customization.md)

# 外觀自訂

執行時、保留嘅外觀控制喺偏好設定 ▸ 外觀：主題、密度、主色調（預設 + 自由選擇器）、完整 UI 字體自訂、一個即時 MD3 標記預覽同埋一鍵重置到預設。所有控制適用即時，凡係可行。

## 行為

| 控制 | AppConfig 鑰 | 適用透過 |
| --- | --- | --- |
| 主題（淺色同埋深色） | `dark_color_mode` | `apply_dark_mode` ⇒ 完整重新主題走 |
| 密度（舒適同埋緊湊） | `ui_density` | `MD3::Metrics::setDensity` + `refresh_md3_appearance` |
| 主色調：6 預設色樣 | `ui_accent_seed` | `MD3::setAccentSeed`（衍生 6 色調角色用於淺色同埋深色） |
| 主色調：**自訂…** 選擇器 | `ui_accent_seed` | 相同管道 ， [MD3 無限選擇器帶顏色翻譯器](color-picker-translator.md)供應任何 sRGB 種子 |
| 字體族（安裝 + 束綁 Roboto、CJK 安全後備） | `ui_font_family` | `Label::rebuild_fonts` |
| 文字大小（細小同埋預設同埋大） | `ui_font_scale` | `Label::rebuild_fonts`（尺度乘以 MD3 類型坡道） |
| **即時 MD3 預覽面板** | ， | 重新繪製透過 `StateColor::semantic` + `Metrics::active()`喺每個重新整理 |
| **重置外觀到預設** | 寫所有上面嘅 | 重新同步執行時狀態喺一個重入守衛下 |

- 自訂選擇器保留預設色樣環真實：選擇一個顏色相等一個預設，燃亮該色樣；任何其他清除所有環。
- 重置恢復密度=舒適、種子=`#146c2e`（品牌 ， 清除主色調覆蓋）、字體族=預設、尺度=1.0。主題係刻意被排斥（一個淺色同埋深色翻轉中期重置係有破壞性）。程序重新選擇執行喺一個守衛下因為 `MultiSwitchButton::SetSelection` 發出佢嘅選擇事件；守衛保留重新發射處理程序從重新保留中期重置。

## 配置

所有鑰生活喺 AppConfig 嘅 `app` 部分同埋被應用喺啟動喺兩個同步位置：`GUI_App::on_init_inner` 同埋 `PreferencesDialog::apply_persisted_md3_appearance`（無效值正常化到預設）。

## 失敗模式

- **無效保留種子同埋尺度** → 正常化到品牌種子同埋限制尺度喺讀時。
- **密度同埋半徑即時重新佈置限制**：視窗構建在密度改動前重新繪製，但完整重新佈置只在重啟後（喺偏好設定源中文件；與密度注記一致）。
- **用戶字體缺失 CJK 字形喺一個 CJK 語言模式** → 自動後備到地區碼 CJK 面（冇豆腐）。

## 安全考量

字體面被驗證對照被列舉系統字體表，使用前；喺 Windows 上、字體到達 `wxGraphicsContext` 路徑係嚴格驗證（列舉器檢查、每面快取）因為 GDI+ 堆損壞喺未註冊可變字體 ， 見純 GDI MaterialIcon 渲染理由。

## 每元素覆蓋

上面嘅控制設定全球主題。任何單個元素可以被重新風格喺頂部 ， 右鍵點擊 ▸ **編輯外觀…**、Shift 同埋右鍵點擊或 **Ctrl+Shift+E** 喺專注控制 ， 透過被描述喺[每元素外觀編輯器](appearance-editor.md)嘅錨定非模態編輯器。該等覆蓋生活喺 `data_dir()/appearance/element-styles.json`、解析透過命名預設同埋重置獨立自呢個頁面嘅*重置外觀到預設*。

## 驗證

- 編譯清潔，在反方面審查後；審查發現（重置重入、無效 GC 字體守衛、自訂選擇器環真實性）固定。
- 外觀頁同埋每個控制被捕捉，每個按鈕喺截圖矩陣下，`docs/screenshots/appearance/`。
