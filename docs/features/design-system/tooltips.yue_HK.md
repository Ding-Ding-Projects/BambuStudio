---
translation-of: tooltips.md
source-sha256: 24a63f327f1d410fea8e5c7e08019bffc1193c3f7f34a54e3c1a84f0f3acd398
review-status: agent-drafted
---

> 英文原文：[Tooltips](tooltips.md)

# 工具提示

Windows 應用程式嘅每個工具提示都係 Material 純文字工具提示：InverseSurface 底色加 InverseOn 文字（淺色主題係 `#2f3036`
底配 `#f1f0f7` 字，深色主題係 `#e3e2e9` 底配 `#2f3036` 字）、套件嘅細字型、8 x 4 DIP 留白，喺 Windows 11 仲會有細圓角。

## 點做

wxWidgets 所有工具提示都經同一個共用嘅 Win32 工具提示控件（`wxToolTip::GetToolTipCtrl()`）顯示。用佢嘅視覺樣式畫嘅時候，
佢係系統嘅淺色方框加系統字型，唔理主題係乜。`src/slic3r/GUI/GUI_App.cpp` 入面嘅 `style_tooltips_md3()` 移除視覺樣式，
令控件用畀佢嘅顏色填色，再設定 Material 嘅顏色、留白同字型。`GUI_App` 喺主視窗出現之後套用一次，每次換主題之後喺
`force_colors_update()` 再套用，所以淺色同深色之間切換會即刻生效。

程式自己畫嘅工具提示，即係 3D 畫布上嘅 ImGui 工具提示同開關掣嘅豐富工具提示，本身已經用 Material 角色。

## 驗證

- `node --test ui-md3/tests/tooltips.test.mjs` 檢查樣式同套用嘅位置。
- `scripts/md3/check-tooltips.py` 喺隱藏桌面用發佈套件執行：打開智能家居，用 post 出去嘅 `WM_MOUSEMOVE` 將指標移到其中一粒掣
  上面，擷取打開咗嘅工具提示，再將佢最常見嘅顏色同主題嘅 InverseSurface 比較。冇工具提示打開，或者顏色係系統嘅，就會失敗。
