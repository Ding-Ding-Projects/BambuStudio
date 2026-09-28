---
translation-of: README.md
source-sha256: 219503d76e8ebda6842ef2327305594a1e21371533de7f485063c6e297e3dbd8
review-status: agent-drafted
---

> 英文原文：[G-code preview](README.md)

# G 碼預覽

呢個類別記錄切片**預覽**頁面 ， 刀具路徑檢視埠、佢嘅色彩計劃圖例，同喺板被切片後顯示嘅運輸/層控制。（對於預匯入 3D 模型檢視器由 MakerWorld「下載同開啟」流程使用，睇 [`../model-preview/`](../model-preview/) 反而 ， 該係一個不同介面。）

- [刀具路徑色彩計劃圖例](toolpath-legend.md) ， ImGui 圖例疊加喺預覽檢視埠：色彩計劃選擇器、每墨水 `FILAMENT | MODEL` 使用表、灰色更改時間/成本摘要行、選項晶片，同時間估算卡。

冇 Postman 收集適用：呢個類別公開冇 HTTP API。
