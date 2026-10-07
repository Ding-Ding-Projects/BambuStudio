---
translation-of: workspace-atlas.md
source-sha256: 436566c860a78c8adccf261b815ef442778861f89ac2f3ca3ee9cea71a9a3e7a
review-status: agent-drafted
---

> 英文原文：[Studio Atlas workspace composition](workspace-atlas.md)

# Studio Atlas 工作區組合

呢個外觀／排版單元改 Project 內嘅原生 `WorkspacePanel`，基準 `3c8fe2708ba03470c1b171e2fce12fcbdfaab0fc`。Project 主頁、共用控件、全域配色、工作區引擎同實體打印不變。

## 真實頁面清單

原有五頁次序同索引不變：

| 頁面 | 原有控件同新組合 |
| --- | --- |
| Overview | 章節下保留名稱／數目摘要，Rename 同時區／提醒操作獨立換行 |
| Files | 三欄成員／項目／可編輯來源表放卡內，Open selected project 主要操作，加檔操作保留 |
| Checklist | 原勾選清單、Add／Edit／Move／Export、章節同換行操作頁尾 |
| Notes | 同一 `TextArea` 及內部 `wxTextCtrl`，持續標題、可讀正文、有用最小編輯高 |
| Calendar | 同一月份控制同四欄議程，量度足夠時並排，否則堆疊，提醒／規劃／匯出保留 |

面板冇 History 子頁。`save_member` 仍經原 staging 同原子 bundle-save 保留有歷史嘅 3MF，冇捏造歷史瀏覽器，亦唔代表獨立項目歷史已重設計。

## 版面同結構

每頁係支援鍵盤走訪嘅原生卡，外面有垂直捲動頁；book／tab 索引不變。最低層背景、outline-variant、密度形狀／間距、16 點角色章節同舒適／緊湊正文字型。全域 New／Open／Save 同 Overview、Files、Checklist、Calendar 操作採換行 sizer；Save、Open selected project、Add checklist item、Add planned print 強調，其餘次要或外框。文字同真實操作不變，冇隱藏或放入虛構選單。

表格／清單有最小內容高，長翻譯或換行令外頁可捲；內部表格、清單及文字捲動保留。日曆斷點由真實月份控制最佳寬、目前間距同 360 DIP 議程區組成，只改方向／邊距，唔改日期或資料。

五棵控件樹建立前忽略尺寸事件，reflow 有遞迴保護。主題／DPI 更新本地角色、字型、間距、操作同分頁，唔重載用戶資料或草稿。真實事件循環、字體、DPI 同即時密度仍待驗證。

## 保留行為

同基準比較 29 個行為／生命週期方法內容不變，包括 bundle 開啟／儲存、staging 擁有權、成員 ID、選取交接、帶歷史儲存／復原、清單次序、筆記刷新、日期時區、提醒、延後／關閉／啟用同 JSON／CSV／ICS。`refresh_overview` 保留原標籤賦值，只尾加視覺 `reflow()`，fixture 只准呢個鉤子。

全部事件綁定及回呼保留，包括筆記即時草稿／修改標記、勾選完成、列啟動、月份同分頁選取。檔案／議程欄宣告、次序、選取樣式不變，全部顯示文字重用原翻譯鍵。冇新引擎、檔案操作、持久化、計時器或打印命令。

## 有限驗證

`node --test tests/workspace/workspace_panel_atlas.test.mjs` 八項來源保留檢查比對基準、五頁身份、派發目標／欄位，拒絕視覺生命週期內改用戶資料／日期／草稿及新翻譯鍵。負向 fixture 改筆記回呼或向 reflow 注入草稿修改，都必須拒絕。

只對該命令設 `WORKSPACE_ATLAS_MUTATE_CALLBACK=1` 可以真實觀察失敗，只改記憶體 fixture，唔改來源；清除後通過。

### 固定寬度 Overview 內容生命週期

初版 `7ef2efed0e58ba6def38adee061349afc106f533` 喺 `refresh_overview` 更新換行文字後冇排版，改名或開 bundle 可以喺冇尺寸事件時改高度，留下過時卡片／捲動範圍。修正喺原賦值後呼叫受保護 reflow，先清 Overview 最佳尺寸及排版，再重算各頁捲動範圍。`m_ui_ready` 同 `m_reflowing` 保留。

`WORKSPACE_ATLAS_SOURCE_REVISION=7ef2efed0e58ba6def38adee061349afc106f533` 以現行檢查測舊來源，六項通過、兩項失敗、退出 1，包括 `Overview content must trigger reflow without a size event`；清除後八項通過、零失敗。新案例按真實呼叫次序，用固定寬度短→長→短文字高模型，冇尺寸事件，亦確認 Rename 同開啟／刷新用同一鉤子。係確定性來源協定模型，唔係原生控件、字體或像素證據。

原生編譯、事件生命週期、鍵盤、讀屏、文字 fit、真實捲動、動態同視覺一致未驗證。冇啟動程式、安裝或打印機，冇擷圖；完整尺寸／語言／主題／密度／比例矩陣待辦。

## 子頁面邊界同還原

原文字／選檔對話框、引導輸入、日期／時間揀選同驗證另有工作。共用 `ListBox` 仍擁有列繪畫及長標籤縮短，唔宣稱全部文字可見；`TextTabbar` 仍擁有溢位／鍵盤／繪畫。每頁搜尋／Regex、豐富列控制、歷史等缺失功能保留未完成清單，冇新增引擎或空殼。獨立歷史、Project 主頁、內嵌內容及標準工具各自擁有。

本單元同功能或打印修改分開。審查後還原外觀提交可恢復原組合，同時保留較早導覽、配色、Print、AMS 同建置。先審查後續呼叫者及文件；冇設定遷移、工作區重設、bundle 重寫或自動回退。
