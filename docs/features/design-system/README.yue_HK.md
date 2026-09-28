---
translation-of: README.md
source-sha256: 25487019793249e616071a2a3676f8897031bf41d39ff44871a915098f9bb49f
review-status: agent-drafted
---

> 英文原文：[Design system](README.md)

# 設計系統

呢個分類記錄點原住民 wxWidgets/OpenGL 應用程式消費內建 Material
設計 3 設計系統。

- [內建 Material Design 3 設計系統](md3-design-system.md)──令牌真實來源、
  地面顏色/類型/指標遷移、情境計劃、字體、故障模式同對比
  審計結果。
- [MD3 對比登記](md3-parity-register.md)──標準元素對元素符合
  登記同波計劃驅動結構解剖遷移。登記本身攜帶
  活躍完成 / 偏差 / 開啟計數；查閱佢而唔係任何
  快照其他度。
- [三向量軌道 SVG 完成](gizmo-rail-svg-icons-completion.md)──有界 34 資產 MD3 令牌
  覆蓋層對剩餘語意三向量複合、附帶原住民
  執行時證據仲然要求之前對比列可移動從部分到完成。

- [工具包小部件添加喺每元素掃掃](kit-widgets-2026-09.md)──LabeledRadioButton 同
  RadioGroup、TextArea、ListBox、Button::SetIconBitmap 同
  預設材料按鈕。
- [執行時佈局探測](layout-probe.md)──關閉預設 NDJSON 行者
  嗰個發現飢鍵盤列、零大小控制同裁剪標籤機械、同佢嘅
  報告讀者。
- [佈局裁剪清單](clipping-inventory.md)──每個發現裁剪缺陷附帶佢嘅
  元組、原因、修正提交同擷取對；機械檢查所以一行唔能
  聲稱證據佢缺乏。
- [主題表面顏色喺 StaticBox 卡](themed-surface-colors.md)──點一個卡得到佢嘅填、
  為乜嘢 `SetBackgroundColorNormal()` 可無聲做冇、同過時建造者時間
  視窗背景後面淺盤喺深色模式。
- [生成視覺展示](generated-visual-showcase.md)──圖像套件分享由
  互動應用程式、GitHub 頁面登陸頁面同社交預覽、包括
  載入、可存取性、部署同驗證行為。

## 設計源

標準倉庫設計源係 [`ui-md3/design-system/`](../../../ui-md3/design-system/)。
令牌值嗰度符合 `src/slic3r/GUI/Widgets/MD3Tokens.hpp` 精確；標題係原住民
真實來源嗰個 C++ 代碼解析對抗。

## Postman 集合

不適用。設計系統係一個編譯時令牌同排字層對桌面
應用程式；佢暴露冇 HTTP 或 API 表面、所以冇 Postman 集合
提供對呢個分類。
