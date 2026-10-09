---
translation-of: native-preferences-setup-atlas.md
source-sha256: e3ca4580c05c6e2ac9846cff4e7a3082a4f911d3049a3821e599785b304d072c
review-status: agent-drafted
---

> 英文原文：[Studio Atlas: project, preferences and setup composition](native-preferences-setup-atlas.md)

# Studio Atlas 項目、偏好設定同設定流程

呢個純外觀單元按 `design/workflow-refresh.md` 同 `native-project-preferences-and-setup` 範圍，由 `be5e1205dcdad8f372d2ba63f367dbd7977c29c4` 開始，改六個原生入口，冇改內嵌樣式或共用組件實作。

## 頁面清單

| 頁面 | 組合 | 保留流程 |
| --- | --- | --- |
| Project | 圓角有邊目的地卡，Online projects／Workspace 操作換行，工作內容喺下面 | 網頁備援、工作區開啟、導覽、腳本訊息 |
| Preferences | 可停靠章節列旁邊有獨立搜尋／內容卡，最低層閱讀面、16/24/16 DIP 標題／列／右邊距、換行頁尾，匯出次要底色、重設降低強調 | 章節 ID、停靠／次序／固定／群組、搜尋、草稿、全部選項及回呼 |
| Schedules | 規則卡包含標題、說明、時區、搜尋、清單、換行操作、詳情及狀態；`Head_20`／`Body_13`，Add 主要、Edit 次要、其餘較低強調 | 搜尋、選取、啟用、次序、確認、持久化、排程監聽 |
| Calibration | 分開外層、低層導覽同最低層裝置選擇，間距跟密度 | 裝置選取、模式 ID、頁面註冊、輪詢 |
| 校準精靈 | 最低層捲動內容、低層流程背景，以密度內距分開 | 置中、雙向捲動、流程路由、校準引擎 |
| 設定精靈 | 共用 `Head_20` 目前步驟標題、語意文字、密度間距、獨立捲動表面；Next／Finish 填色、Back 外框、Cancel 文字 | 索引、自訂打印機、耗材、導覽、ID、驗證、完成／取消 |

Preferences 保留 Appearance、Schedules、General、User、3D、Other、條件可用嘅 Developer Tools 同 Settings draft，亦保留 Model import、Mouse Settings、Import Settings、Project、Online Models、AI printer watch、Developer Mode、檔案關聯、Log、Host Setting 等子標題。搜尋索引使用嘅標題字型不變，冇新增、刪除或改名選項及翻譯鍵。設定精靈保留打印機／材料、自訂打印機、韌體、床形、直徑及溫度，冇改值或流程次序。

## 互動同動態

新容器保留 `wxTAB_TRAVERSAL`，原控件仍擁有焦點、輸入、停用繪畫及減少動態回饋。冇新增計時器、循環、透明度、硬件或非同步流程。Project／Preferences 操作換行時唔用爭位伸展空格。Preferences 頁尾喺設定捲動區外；排程詳情仍喺內容捲動區內。

## 來源驗證

`node --test tests/native_preferences_atlas.test.mjs` 七項有限檢查保留六個入口所有綁定、選項註冊、值及身份呼叫令牌同字面值，比較 Project、Schedule、Calibration 引擎尾段，檢查卡片擁有權、換行、操作層次，拒絕刻意替換回呼及移除鍵盤走訪。須有本地基準提交，唔會編譯或執行 C++。

冇啟動程式、瀏覽器、安裝程式或打印機，冇完整建置、擷圖或版面量度。原生最小尺寸、雙語、主題、比例、鍵盤、焦點返回同動態仍未驗證。禁止啟動令 Material Designer 即時流程不可用，已提交規格只係設計來源。

## 餘下實作邊界

`WorkspacePanel`、`ConfigWizardIndex`、`CalibrationWizardPage` 及子頁面不在呢個單元。校準只改外殼及流程背景；規則編輯器／條件欄位、Preferences 專用控件／重設詳情保留內部排版，智能家居同認證不變。缺少嘅標準功能仍係獨立未完項。保留可獨立還原嘅外觀提交，唔移除建置、序列化、排程、校準或打印修正；整合負責人提供後續證據。

## 卡片生命週期同巢狀搜尋修正

初版 `8b0b5e6da19d0ce877a3c5eb770de6945a516e77` 有兩個缺陷：明確實體像素圓角停用預設 DPI 重算，新排程卡亦令註冊列移到更深層，但索引仍只看直接項目。

後續三張卡都用 `StaticBox::SetDensity`，之前唔覆蓋半徑。真實 Preferences 索引經 wx sizer／window 轉接器用 `PreferencesSearchTraversal.hpp`：只進入確實含註冊列嘅容器，遇到確切註冊 sizer／window 身份停止，保留未註冊直接列同原可見性，唔將結構卡當成可隱藏列。子列篩選唔會隱藏自己張卡。原有父相對捲動已累積祖先位置，毋須替換。

喺 MSVC 開發環境執行 `node tests/native_preferences_behavior.test.mjs`，會喺倉庫外編譯暫存無視窗 fixture，包含真實 `build_search_index`、圓角方法同三個卡片初始化語句；記憶體替身提供身份及 DPI 換算，冇建立原生視窗。編譯器由 `tests/native_fixture_compiler.mjs` 揀：Windows 用 MSVC `cl.exe`，其他主機用 `$CXX`（預設 `c++`），C++17 加 `-Wall -Wextra`，咁 Linux 合約檢查都會跑同一個 fixture，唔會因為冇 `cl.exe` 就停低。

附加 `--baseline=8b0b5e6da19d0ce877a3c5eb770de6945a516e77 --case=rows`，舊版出現 `nested card was indexed instead of its rows`；`--case=radius` 出現 `card initializer pinned radius`。修正後九個案例通過：精確鍵／文字匹配、篩選／重設、隱藏祖先、註冊停止邊界、未註冊複合列、結構包裝、空位、三初始化×四比例、密度同明確覆蓋語意。較早七項亦通過。

MSVC 對原有 `SetCornerRadius` 參數遮蔽成員報 C4458，冇壓制或當成新缺陷。證據只驗證隔離產品邏輯，唔代表原生繪製、鍵盤、真實 DPI 事件或完整建置。
