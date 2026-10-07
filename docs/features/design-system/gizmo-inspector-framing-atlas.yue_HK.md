---
translation-of: gizmo-inspector-framing-atlas.md
source-sha256: ffb1391af925e82017413f836bfcab72e1900b53c08b7b734f8f02e5e0f45891
review-status: agent-drafted
---

> 英文原文：[Studio Atlas common floating inspector framing](gizmo-inspector-framing-atlas.md)

# Studio Atlas 共用浮動檢查面板框架

唯一修改嘅產品源碼係 src/slic3r/GUI/Gizmos/GLGizmoBase.cpp 入面嘅 GizmoImguiBegin 同 GizmoImguiEnd。基線係 b5c42ce78a8e021509bdc504e5a40ab62d9cd8e4，編輯前已快轉合併到獨立實作分支。

## 覆蓋及外觀

十二個現有成對呼叫者都喺 src/slic3r/GUI/Gizmos/：GLGizmoAdvancedCut.cpp、GLGizmoAssembly.cpp、GLGizmoBrimEars.cpp、GLGizmoFdmSupports.cpp、GLGizmoFlatten.cpp、GLGizmoFuzzySkin.cpp、GLGizmoMeasure.cpp、GLGizmoMmuSegmentation.cpp、GLGizmoMeshBoolean.cpp、GLGizmoSeam.cpp、GLGizmoSVG.cpp 同 GLGizmoText.cpp。物件變換面板唔經呢個包裝函式，唔屬於今次範圍。

框架按畫布主題解析不透明 SurfaceContainerLowest 底板同 OutlineVariant 邊線。WindowRounding 跟隨現有密度同畫布縮放。一條短 Primary 標題線使用原有頂部內距嘅內半部，位於第一行內容之上；現有空間不足就唔畫。冇新增內容項目或排版空間，原有呼叫者邊框同本地控制樣式仍然有效。

Begin 原樣轉交名稱同旗標，並傳回原有可見性結果。End 保留原有寬度記錄同結束呼叫，之後還原兩個顏色同一個圓角值，Begin 傳回 false 都一樣。冇改游標、項目寬度、內距、輸入身份、變換、工具計算、回呼、選取、相機、數據配色、計時器或畫格排程。冇新增動態效果，現有共用動畫同減少動態效果行為不變。

## 驗證及限制

執行：node --test tests/gizmo_inspector_framing.test.mjs

三項源碼檢查通過：呼叫旗標／傳回值／樣式生命週期、十二個面板嘅成對路徑，以及正規化雜湊證明兩個包裝函式以外嘅基底工具源碼不變。刻意將轉交旗標改成零會令第一項檢查失敗；還原原有位元組後三項再次通過。呢啲係源碼合約，唔係原生編譯或視窗互動結果。

冇做完整建置、啟動、截圖、硬件操作或發佈。明暗主題對比、實際標題線可見性、文字空間、全部面板狀態、縮放、密度、語言組合同執行時樣式還原仍未驗證。可以獨立還原呢個更新，保留之前嘅渲染器、通知、內嵌介面同物件變換工作。
