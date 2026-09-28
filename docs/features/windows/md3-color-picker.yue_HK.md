---
translation-of: md3-color-picker.md
source-sha256: 7b91a21b11c207fc0002e591cee7a111a9e0f428cf6b41623e1d857122712e54
review-status: agent-drafted
---

> 英文原文：[Material color picker & color translator](md3-color-picker.md)

# Material 色譜選擇器 & 色譜轉譯器

**表面：**偏好設定 ▸ 外觀 ▸ 重音「+」磚 → `MD3ColorPickerDialog`
（`src/slic3r/GUI/Widgets/MD3ColorPicker.{hpp,cpp}`），取代本地
`wxColourDialog`。

## 行為

- 一個**連續（無限）選擇器**，使用 MD3 令牌設計：當前色調嘅飽和度 / 值欄（每份 RGB 顏色都可以到達）、連續色調條、同埋一個 **Material 色調階梯** 、 十一個色調（5…95）嘅當前選擇作為一鍵快速選擇。梯級會為每個色調重新衍生，所以對色輪上嘅任何顏色都有一份新鮮嘅 Material 坡道。
- 一個實時預覽芯片同埋一份 `#RRGGBB` 十六進制欄，兩向同步（輸入完整十六進制會跳轉選擇器；選擇會更新欄）。
- **色譜轉譯器**：一份翻譯列會展示同一顏色喺每份記號裏面（CSS 名稱、HEX、HEX8、RGB、RGBA、HSL、HSLA、HSV、HWB、XYZ、Lab、LCH、OKLab、OKLCH、CMYK），各個帶着複製按鈕、旁邊係一個透明度滑塊、一份「輸入任何格式」欄、一份色域警告同埋一份 WCAG 對比度讀數。睇 [色譜選擇器轉譯器](color-picker-translator.md) 以取得完整契約同埋精度註。
- OK 會將選擇饋送進同一個 `MD3::setAccentSeed` 管道，就好像六份預設樣本咁樣；取消冇改變任何嘢。

## 驗證

- 編譯進 `libslic3r_gui`；由重音「+」磚透過 Lowlevel MCP 駕駛方法無頭打開；十六進制往返同埋色調階梯選擇會實時更新預覽同埋轉譯器。
