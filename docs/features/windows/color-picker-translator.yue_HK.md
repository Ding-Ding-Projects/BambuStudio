---
translation-of: color-picker-translator.md
source-sha256: 5c922862d0da956a96186c1c549803200e7c744a805783368555ea172e36fc58
review-status: agent-drafted
---

> 英文原文：[Color picker translator (infinite picker)](color-picker-translator.md)

# 顏色選擇器翻譯器（無限選擇器）

**表面**：每個 `MD3ColorPickerDialog` ， 偏好設定 ▸ 外觀 ▸ 主色調 **自訂…**、預設編輯器嘅 `ColourPicker` 欄位（`Field.cpp`）同埋批量長絲顏色行動（`BulkFilamentDialog.cpp`）。

**代碼**：`src/slic3r/GUI/Widgets/MD3ColorPicker.{hpp,cpp}`（對話框）同埋 `src/slic3r/GUI/Widgets/ColorSpaces.hpp`（僅標頭數學、命名空間 `MD3::Color`）。

本文擴展了[材料顏色選擇器及顏色翻譯器](md3-color-picker.md)，描述飽和度同埋值欄位、色調條同埋材料色調階。下面嘅一切係額外嘅。

## 行為

- **無限選擇器**。飽和度同埋值欄位同埋色調條到達每個 sRGB 顏色；對話框入面唔係任何有限嘅色樣網格。該欄位同埋條係鍵盤可操作嘅：方向鍵按 1% 推動、Shift 按 10%。
- **不透明度滑塊**（0–100%）使用共用嘅 MD3 `Slider`（可聚焦、螢幕讀取器命名為「不透明度」）。alpha 係攜帶喺 `GetColour().Alpha()` 中；只讀取 `#RRGGBB` 嘅呼叫者係未受影響。預覽芯片展示顏色複合喺對話框表面上。
- **翻譯欄**。當前顏色用每個支援嘅符號寫入，每一個喺一個僅讀欄位，用佢自己嘅複製按鈕（可訪問名稱 `複製 HEX`、`複製 OKLCH` 等）。行，按次序：

  | 行 | 格式 | 備註 |
  |---|---|---|
  | 名稱 | CSS 顏色名稱 | 僅精確 8 位元匹配；當冇時為 em dash |
  | HEX / HEX8 | `#RRGGBB` / `#RRGGBBAA` | HEX8 入面嘅 alpha |
  | RGB / RGBA | `rgb(r, g, b)` / `rgba(r, g, b, a)` | 0–255、alpha 0–1 |
  | HSL / HSLA | `hsl(h, s%, l%)` | 度數、百分比 |
  | HSV | `hsv(h, s%, v%)` | 同埋解析為 `hsb()` |
  | HWB | `hwb(h w% b%)` | CSS Color 4 語法 |
  | XYZ | `xyz-d65(x y z)` | D65 白色、Y(白色)=1 |
  | Lab / LCH | `lab(L a b)` / `lch(L C h)` | CIELAB、L 0–100 |
  | OKLab / OKLCH | `oklab(L a b)` / `oklch(L C h)` | L 0–1 |
  | CMYK | `cmyk(c%, m%, y%, k%)` | **樸素**公式、唔係一個印刷設定檔 |

- **輸入任何格式**。一個欄位解析上面嘅任何符號（逗號或空格分隔、可選 `/ alpha`、`%`/`deg`/`turn` 單位、`none`）加 148 個 CSS 顏色名稱，同埋跳轉選擇器到佢。內聯狀態行命名活動顏色空間（`解析為 OKLCH`）或說明為乜嘢文字係未被理解。解析係即時每個按鍵；唔係冇 Enter 按。
- **色域標籤同埋警告**。`活動空間…色域…` 行告訴哪個空間顏色最後被定義喺同埋佢係否位於 sRGB 內。當一個輸入嘅 Lab/LCH/OKLab/OKLCH/XYZ 值位於 sRGB 外面時，狀態行轉到錯誤角色，讀取*「超出 sRGB 色域：選擇器展示最近嘅 sRGB 顏色（剪輯）」*，一個非阻塞內聯文字，從唔係對話框。剪輯嘅值係選擇器展示嘅同埋 OK 返回嘅。
- **對比度讀取**。兩個 WCAG 2.x 比率對照一個前景同埋背景對：選中嘅顏色作為*呼叫者文字下方嘅背景*，同埋作為*呼叫者表面上嘅文字*。每個比率攜帶佢嘅級別（AAA / AA / 僅大文字 AA / 失敗 WCAG）。Alpha 係複合喺表面上，測量前，所以一個半透明拾取報告眼睛會看到嘅。

## 配置

- 對比度對係一個構造函數參數：

  ```cpp
  MD3ColorPickerDialog dlg(parent, initial);                      // OnSurface vs Surface (MD3 tokens)
  MD3ColorPickerDialog dlg(parent, initial, { text_colour, surface_colour });
  ```

  `MD3ColorPickerDialog::defaultContrastContext()` 返回預設對。任一半可以係 `wxNullColour` 去跳過該比率。
- 冇任何嘢被對話框自己保留；呼叫者儲存拾取如前（`ui_accent_seed`、預設選項、分段長絲顏色）。
- 對話框冇佢自己嘅設定同埋冇任何嘢去選擇退出；三個語言模式同埋兩個有趣級別滑塊通過 `_L()` 申請，如每個其他表面。

## 精度

喺標頭內文件同埋由 `tests/color_spaces` 斷言：每個 sRGB ⇒ 空間 ⇒ sRGB 往返精確地重現 8 位元輸入。喺雙精度中殘差係 ~5e-5 用於 XYZ/Lab/LCH（已發佈 7 數字 sRGB 矩陣唔係確切逆），~1e-5 用於 OKLab/OKLCH 同埋 ~1e-9 用於 HSL/HSV/HWB/CMYK。色調係報告為 0 喺零色度。

## 失敗模式

- **未識別文字** ， 狀態行（錯誤角色）：*唔係已識別顏色…* 列出接受嘅符號；選擇器保留佢嘅當前顏色。冇模態、冇嗶聲。
- **超出色域輸入** ， 解析、標記、展示剪輯，帶上面嘅警告。返回嘅顏色永遠係一個有效嘅 sRGB 三重奏。
- **剪貼板忙碌** ， 複製無聲咁當 `wxTheClipboard` 唔能被打開時做冇嘢；一個成功嘅複製係喺狀態行上確認嘅。
- **冇 CSS 名稱嘅顏色** ， 名稱行展示一個 em dash 同埋佢嘅複製按鈕係禁用嘅。
- **超大輸入** ， 任何格式欄位係封頂喺 128 個字符，最長合法符號良好喺下面。

## 安全考量

- 所有解析同埋轉換係本地、分配有界（一個固定數量嘅數字標記；`std::strtod` 喺短字串上）同埋從唔觸及網絡、檔案系統或應用設定。
- 解析器接受冇代碼、冇 URL 同埋冇逃脫序列；任何冇被識別符號嘅嘢係拒絕、唔係猜想。
- 複製只寫顯示嘅文字到系統剪貼板、喺用戶嘅明確點擊上。

## 驗證

- `tests/color_spaces/color_spaces_tests.cpp`（Catch2 目標 `color_spaces_tests`、註冊喺 `tests/CMakeLists.txt`）：往返白色、黑色、中灰同埋 sRGB 原色通過每個空間；地標值（D65 白色、L=100、純紅色色調）；一個 P3-ish `oklch(0.85 0.3 145)` 標記剪輯；命名顏色查找兩種方式；黑色對白色對比 = 21；每一行嘅格式；每個符號嘅解析，包括 `/ alpha`、百分比、`hsb()`、`device-cmyk()`、名稱同埋八種垃圾。編譯同埋用 MSVC 獨立執行：**所有測試通過（223 個斷言喺 8 個測試情況中）**。
- `MD3ColorPicker.cpp` 通過 `cl /Zs`，帶 `libslic3r_gui` 包含目錄同埋定義。
- 佈局預算：兩列（左邊選擇器、右邊十五翻譯行）保留對話框內 1000×600 DIP；狀態同埋對比行預留佢們嘅包裹高度，所以列從唔會重新流動。
- 構建對話框嘅無頭截圖唔係此改動嘅部分（冇完整構建被執行）；視覺煙霧食譜喺[本地視覺煙霧測試](native-visual-smoke.md)涵蓋佢喺下一次構建。

## 建議文章

- [材料顏色選擇器及顏色翻譯器](md3-color-picker.md) ， 飽和度同埋值欄位、色調條同埋色調階本文構建上。
- [外觀自訂](appearance-customization.md) ， 選擇器供應主色調種子嘅地方。
- [鍵盤、輔助及響應式 GUI 可訪問性](gui-accessibility.md) ， 聚焦、命名同埋窄寬度規則對話框遵循。
- [批量長絲行動](bulk-filament-actions.md) ， 選擇器嘅第二個呼叫者。
