---
translation-of: process-settings-full-tree.md
source-sha256: 6fbe23e5f1f8feb2cc252afbd3730cb0943e50d74b44a4f98858aef335dbdc8d
review-status: agent-drafted
---

> 英文原文：[Every process setting shown, no Simple/Advanced filter](process-settings-full-tree.md)

# 所有流程設定顯示、無簡單同埋進階篩選

## 行為

- 準備側邊欄嘅流程部分係由首次啟動嘅完整設定樹。緊湊嘅「簡單設定」卡同埋佢嘅「進階設定」同埋「簡單設定」切換已移除。
- `GUI_App::get_mode()` 對每個儲存嘅 `user_mode` 回覆進階除咗 `develop` 之外、所以冇任何選項隱藏喺一個模式後面。「進階」標籤同埋模式開關已唔再建立喺設定標題或任何設定標籤到。
- 一個儲存嘅 `sidebar_process_advanced` 值來自一個舊配置檔有目的地被忽略。
- 側邊欄主體係唯一嘅捲動表面。樹喺佢嘅完整內容高度 (`ParamsPanel::fit_page_to_content`) 被託管同埋主體嘅尺寸被佈局喺虛擬高度上 (`update_sidebar_scroll_body` 喺 `Plater.cpp`)、所以每個選項列、一直到「其他」嘅最後列、都可以通過捲動側邊欄到達。喺可見客戶端高度上佈局尺寸 (簡單 `Layout()`) 會將樹壓扁對着視窗底部、同埋捲動條指向空白空間；那係呢個規則存在以防止嘅缺陷。
- 喺對象模式、對象列表唔會拉伸。佢嘅高度跟隨佢嘅可見列 (`Sidebar::fit_object_list_height`：標題、加列、被限制喺 180 dp 同埋 420 dp 之間) 同埋當一個板塊或對象被展開或摺疊同埋當對象被添加或移除時被重新調整。

## 驗證

- 契約：`ui-md3/tests/md3-conversion-contracts.test.mjs`、「每個流程設定都被顯示」。
- 佈局探測：`sidebar-check` 驅動程序命令 (`LayoutProbe.cpp`) 記錄主體嘅客戶端、虛擬同埋內容高度加上最後堆疊子元素嘅底部、同埋當最後子元素唔以虛擬底部結尾或虛擬高度低於內容高度時回覆 `DEFECT`。
- 捕獲：`docs/screenshots/md3-everything/prepare-advanced--en-light-comfortable--after.png`
- 對象模式、捲動到側邊欄主體嘅尾端（整個常用頁面都在檢視中、探測判定係 `ok`）：`docs/screenshots/md3-everything/prepare-objects-bottom--en-light-comfortable--after.png`
  （同一表面嘅頂端：`prepare-objects-top--en-light-comfortable--after.png`）。
  （同一表面喺 1000 x 600 最小係 `prepare-advanced-minimum--...--after.png`）。

## 相關

- [準備側邊欄搜尋](../windows/sidebar-search.md)
- [佈局剪裁清單](../design-system/clipping-inventory.md) （CJ-012 係呢個側邊欄）
