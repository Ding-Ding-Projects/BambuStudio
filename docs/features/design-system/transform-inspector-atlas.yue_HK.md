---
translation-of: transform-inspector-atlas.md
source-sha256: a68b703886fccb8bf1c7451166422f888941b457ba4d0aa659f90eeb895e97ba
review-status: agent-drafted
---

> 英文原文：[Studio Atlas transform inspector](transform-inspector-atlas.md)

# Studio Atlas 變換檢查面板

呢個只涉及源碼嘅外觀更新局限於 Gizmos/GizmoObjectManipulation.cpp，依據基線 2a3528c451f3a5caccdfd7fd9e8d9d8964e4d4c8 所做嘅渲染器盤點。呢份記錄唔代表已完成原生編譯或實際畫面驗收。

## 實際來源及選定範圍

以下路徑都相對於 src/slic3r/GUI/。

- Gizmos/GLGizmosManager.cpp 經 Gizmos/GLGizmoBase.cpp 入面嘅 GLGizmoBase::render_input_window 派發目前工具嘅輸入視窗。基底類別負責錨點轉換同首次渲染失效通知。
- Gizmos/GLGizmoMove.cpp、GLGizmoRotate.cpp 同 GLGizmoScale.cpp 呼叫 Gizmos/GizmoObjectManipulation.cpp 入面對應嘅三個視窗。md3_value_input 輔助函式負責全部十五個數值欄位，axis_header 繪製座標標題，show_align_icon 負責本地對齊工具提示。
- 其餘有源碼定義嘅工具面板分別位於 Gizmos/GLGizmoAdvancedCut.cpp、GLGizmoCut.cpp、GLGizmoBrimEars.cpp、GLGizmoAssembly.cpp、GLGizmoFaceDetector.cpp、GLGizmoFdmSupports.cpp、GLGizmoFlatten.cpp、GLGizmoFuzzySkin.cpp、GLGizmoHollow.cpp、GLGizmoMeasure.cpp、GLGizmoMeshBoolean.cpp、GLGizmoMmuSegmentation.cpp、GLGizmoSeam.cpp、GLGizmoSimplify.cpp、GLGizmoSlaSupports.cpp、GLGizmoSVG.cpp 同 GLGizmoText.cpp。源碼有覆寫函式，唔等於已證明每款打印機模式都有註冊或可以開啟嗰件工具；啟用路徑盤點仍然係另一項工作。
- GLCanvas3D.cpp 負責 LayersEditing::render_overlay，以及打印板、組裝縮圖、組裝預覽、組裝步驟、返回、相機配合、摺疊、塗繪同組裝控制浮層。今次冇改呢啲函式。

選定嘅一致範圍係移動、旋轉、縮放檢查面板同對齊工具提示。冇改共用 Widgets 源碼、原生工作流程、監控介面、內置渲染器或標頭檔。

## 外觀及功能界線

數值文字繼續使用現有等寬字體同 OnSurface 角色。欄位底色使用 SurfaceContainerLow；靜止時邊線用 OutlineVariant，游標停留時用 Outline，啟用時用 Primary。現有輸入函式處理事件之後，邊線先讀取 IsItemActive、IsItemHovered 同最後嘅 GetItemRect 範圍。線寬受現有欄位內距限制，繪製路徑留喺原有矩形之內，唔會新增項目、移動游標、改字體大小、加入填色覆蓋層、攔截輸入或增加動畫。

單靠外層停留或啟用樣式覆寫喺呢度唔會生效：內置 BBLInputDouble 會自行設定 BorderActive，而 InputTextEx 路徑繪製嘅係 FrameBg。所以本地裝飾喺輸入處理後使用實際最終範圍，避免設定一個會被忽略嘅覆寫。已檢視嘅數值路徑冇獨立驗證顏色繪製器；解析、運算式處理同編輯狀態回報保持不變。

座標標題保留 X/Y/Z 數據配色同原有置中方式。中性色欄分隔線畫喺現有行距內，唔佔額外排版空間。本地對齊提示使用 InverseSurface/InverseOn，同提示視窗實際選用嘅 WindowRounding 樣式；樣式推入同還原保持平衡，顯示條件同文案不變。

三個視窗函式保留全部原有游標、寬度、行、位置、單位同回呼陳述式。現有非數值控制繼續使用所屬樣式。變換、解析、範圍、相機、選取同模型修改程式碼都冇改。

## 現有動態效果來源

ImGuiWrapper.cpp 負責各個 context 嘅選單裝飾、彈出視窗同工具提示時間線。menu_decoration_progress 使用有界嘅 100 毫秒回饋，MD3::Motion::reduced 啟用時即時完成。render 將彈出視窗繪製效果限於所屬列表，畫完還原原有頂點顏色，亦只喺有界轉場期間要求額外畫格。tooltip_decoration 有獨立嘅內容／來源鍵控 100 毫秒標記同減少動態效果處理。ImGuiWrapper.hpp 儲存呢啲有界動態對應表；今次全部冇改。

GLGizmoBase 首次渲染嘅失效通知係可見性／生命週期機制，唔係新增裝飾動畫。IMSlider.cpp 負責實際播放計時，兩條路徑都保持不變。今次冇新增計時器、畫布淡入淡出、幾何插值或畫格排程。

## 源碼驗證

現有針對性檢查已通過：

~~~text
node --test --test-name-pattern='gizmo object-manipulation' ui-md3/tests/md3-conversion-contracts.test.mjs
~~~

一項檢查通過。刻意移除一個輔助函式呼叫時，十五欄位清單檢查確實報錯；還原原有位元組後再檢查亦通過。呢項現有源碼檢查證明輔助函式覆蓋範圍，唔係外觀或執行時正確性證據。

基線比較確認全部十五個數值呼叫陳述式同參數、原有 BBLInputDouble 轉交、所有座標軸身份，同每條游標／寬度陳述式都冇變。呢個更新亦附有公開文字掃描同已暫存內容空白檢查。

今次冇執行完整建置、啟動應用程式、截圖、硬件操作或發佈。原生編譯、實際文字／文字游標空間、焦點可見性、停用／唯讀行為、運算式編輯、提示換行、主題／自訂種子色對比，以及支援嘅縮放／語言組合仍未驗證。如唔接受外觀，可以獨立還原呢個外觀更新，同時保留之前嘅渲染器及功能修改。
