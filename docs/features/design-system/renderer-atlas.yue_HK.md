---
translation-of: renderer-atlas.md
source-sha256: a846840b6a6a6aa56174730d0414ed26bad82bfca425808a707655083898f885
review-status: agent-drafted
---

> 英文原文：[Studio Atlas renderer surfaces](renderer-atlas.md)

# Studio Atlas 渲染器畫面

呢次來源外觀更新涵蓋視窗工具列、浮動工具列、ImGui 覆蓋層及提示、Preview 播放控制、圖例標題及統計卡。唔代表原生編譯、視覺一致或完整介面交付。

## 外觀

- 浮動工具列用最低層容器做操作區，保留邊線同陰影；外圓角跟舒適／緊湊密度。
- 工具列喺原項目矩形內畫高層容器懸停層；停用項目冇該層，選取圖示同主要填色仍作準。
- 覆蓋控件用低／高／最高表面分開靜止、懸停、按下，兩種主題一致。選單選取保留情境容器，視窗、欄位同彈出圓角跟密度，內距、項目間距同目標矩形不變。
- 提示配對不透明反向背景、反向文字同密度圓角；雙語、換行、關閉同減少動態裝飾不變，新增樣式推入後全部還原。
- 時間軸手柄、播放控制同標記用 Preview 配色，資料決定嘅耗材、路徑及色塊不變。
- 圖例標題背景限制喺原內容跨度，唔再畫出視窗；強調標記同底部分隔線分開控制及資料，統計卡用最低層容器保留原邊線，排版同計算不變。

## 保留邊界

`BaseRenderer.cpp` 只改 `BaseRenderer::render_legend` 內繪畫。量度、摺疊狀態、回呼、可見性、時間估算、層數同色塊資料不變；切片、硬件、模擬時間、相機、模型選取、滑桿值、路徑緩衝同畫面循環都不變。`GLCanvas3D.cpp` 同全部標頭檔不變。

## 驗證

`node --test ui-md3/tests/preview-overlays.test.mjs ui-md3/tests/preview-counter-font.test.mjs` 八項通過，包括六項覆蓋／幾何來源檢查及兩項翻譯計數字型檢查。刻意將翻譯文字插入數字等寬字型跨度，檢查已觀察拒絕；逐位元組還原後兩項計數檢查再次通過。冇新增只比對繪畫值嘅斷言。

最初稀疏工作樹缺 SVG fixture，補回原有 `resources/images` 讀取範圍後通過。另一原生 wx 提示來源套件缺少範圍外 `scripts/md3/check-tooltips.py`，未能成功執行；亦唔係本次 ImGui 提示驗證。

冇完整建置、啟動、硬件、安裝執行或擷圖。原生編譯、真實懸停／焦點、自訂種子色對比、雙語文字、減少動態、明暗及密度／比例矩陣仍待整合任務嘅建置成品驗證，來源檢查唔等於呢啲結果。

## 提示圓角修正

最初覆蓋 `PopupRounding`，但內置 ImGui 提示旗標冇 `Popup`，實際讀取 `WindowRounding`。封裝而家喺 `BeginTooltip` 前覆蓋 `WindowRounding`，`EndTooltip` 後還原原有單一樣式項。

`renderer-tooltip-rounding.test.mjs` 用符號樣式值計算真實旗標及圓角表達式，比對封裝覆蓋成員同單次推入／彈出生命週期。舊覆蓋已觀察失敗，修正後通過。只係來源合約證據，唔係原生編譯或實際圓角畫面。
