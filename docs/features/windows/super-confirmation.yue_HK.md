---
translation-of: super-confirmation.md
source-sha256: 246f3d446268e0a6ce80c05e0e6c1d0f9c6983162727f3b48007f1e0d77bc52e
review-status: agent-drafted
---

> 英文原文：[Destructive-action super confirmation](super-confirmation.md)

# 破壞性動作超級確認

**表面：** `SuperConfirmGate`（`src/slic3r/GUI/Widgets/SuperConfirmGate.{hpp,cpp}`）、由無工具包狀態機 `SuperConfirmState.hpp` 驅動。佢取代曾經喺應用嘅真正不可逆轉動作前面嘅是同埋否消息框。

## 行為

門係無邊框物料 3 表面（SurfaceContainerHigh、MD3 字幕帶、DWM 圓角）打開**錨喺要求破壞性動作嘅控件旁邊**：當有空間時喺佢下方、否則上方、右方或左方、永遠被限制到顯示且絕唔覆蓋錨。當錨係一個整個面板（3D 檢視、食堂）表面中心喺佢上方代替懸掛喺一個邊緣。冇錨根本佢落回到一個誠實中心模式對話框。

從頂到底佢顯示：

1. **安全事實** 、 呼叫者嘅確切後果句子喺 `Head_16` OnSurface（「每個對象喺每個板上將從呢個項目移除。」）、受影響項目名稱作為一個分點列表（前八個、然後「...同埋 N 更多」）、同埋一個錯誤著色計數列（「7 項將被影響。這不能被撤銷。」）。呢啲字串通過 `_L()` 所以語言模式同埋有趣等級風格聲音；動作、項目同埋計數絕唔被柔和、被幽默重新詞語化或隱藏。
2. **一個舞台字幕** 述說剩下嘅：「第 1 步共 2 步：轉兩個鑰匙。」、「第 2 步共 2 步：滑所有方式到尾端。」、「保持滑動...提前釋放撤銷。」
3. **兩個鑰匙開關** 、 繪製旋轉鑰匙（垂直槽 = 關、水平 = 開、PrimaryContainer 填充當打開）。每個係佢自己可聚焦控件由點擊、`Space` 或 `Enter` 操作、宣告到輔助技術作為檢查按鈕起名「鑰匙 1 共 2 個：轉到武裝」、「鑰匙 2 共 2 個：轉到武裝」同埋一個現場檢查狀態。轉第二個鑰匙打開遞鍵盤焦點到滑塊；轉任何鑰匙關掉下落滑塊回到靜止。
4. **一個危險充電欄** 、 填充 ErrorContainer 如儀式進行（每個鑰匙四分之一、剩余一半追蹤滑動）同埋錯誤危險條紋爬行當鑰匙旅行時。喺減少運動（`SPI_GETCLIENTAREAANIMATION` 關）每個動畫跳到佢嘅結束狀態；條紋爬行絕唔啟動。
5. **完整範圍 `SlideToConfirm`** 、 禁用直到兩個鑰匙打開。拖鑰匙到尾端、或用方向鍵同埋 `End` 行走佢。部分滑動捲回；單獨旅行絕唔授權（狀態機蓋住現場旅行喺 99 百分比同埋只有滑塊自己嘅完成事件到達 100）。
6. **緊急退出** 、 一個色調按鈕係永遠啟用、持有初始焦點（所以 `Enter` 永遠無法確認）、同埋取消。`Escape` 同埋關閉視窗做相同；點擊外一個錨門關閉佢作為取消。

喺授權一個不同完成突發演奏（PrimaryContainer 洪氾欄同埋一個主要圓盤同埋檢查字形生長自中心、`medium2`、強調減弱）、然後門隱藏、**返回焦點到控件當門打開時有佢**（落回到錨）同埋唯一然後報告成功。取消恢復焦點相同方式。

回調只當 `SuperConfirm::State::may_fire()` 係真時激發、兩個鑰匙打開、滑塊喺 100、唔被取消。一個點擊、一次按鍵、一個關閉或一個呼叫者通過 `true` 無法繞過那個謂詞。

## API

```cpp
SuperConfirmGate::Spec spec;
spec.action      = _L("Delete plate");
spec.consequence = wxString::Format(_L("Plate %d and every object on it will be removed from this project."), n);
spec.affected    = { "Benchy", "Calibration cube" };   // names; spec.affected_count overrides the total
if (SuperConfirmGate::Run(anchor_window, spec))      // blocking: nested loop (anchored) or modal (no anchor)
    do_the_irreversible_thing();

SuperConfirmGate::Show(anchor_window, spec,          // callback form: non-modal, self-destroying
                       [] { do_the_irreversible_thing(); },
                       [] { /* cancelled */ });
```

## 門控動作

| 動作 | 呼叫位置 | 錨 | 附註 |
| --- | --- | --- | --- |
| 刪除所有對象（檔案同埋食堂「刪除全部」） | `Plater::reset_with_confirm`（`Plater.cpp`） | 食堂面板（中心） | 列舉每個對象名稱。 |
| 刪除預設 | `Tab::delete_preset`（`Tab.cpp`） | 預設組合框 | 保持原始解釋文字作為後果；第三方打印機自動確認路徑係未變。 |
| 刪除墨水槽 | `Sidebar::delete_filament_with_confirm`（`Plater.cpp`）、由每列墨水選單使用（`GUI_Factories.cpp`）同埋繪圖字符刪除按鈕（`EVT_DEL_FILAMENT`） | 槽嘅組合框 | 名稱槽數同埋預設。大量墨水動作同埋合併保持佢們自己確認同埋直接呼叫 `delete_filament()`、所以一個批次確認一次、唔係每個槽。 |
| 刪除板（板工具欄同埋板懸停動作） | `Plater::confirm_delete_plate`（`Plater.cpp`） | 3D 檢視（中心） | 只當板帶着對象；一個空板損失無嘢同埋被刪除冇門。 |
| 停止打印 | `StopPrintGateDialog`（`StopPrintGate.cpp`） | 模式 | 先前存在相同解剖連鎖（兩個鑰匙、武裝按鈕、滑動、蓋）；見[停止打印安全連鎖](stop-print-interlock.md)。 |
| 輸出同埋導入整個數據資料夾 | `ConfigProfilesDialog` | 喺對話框內 | 先前存在內聯 `SlideToConfirm` 武裝；尚未遷移到兩鑰匙門（見失敗模式）。 |

**有意唔門控：** 從對象列表同埋 3D 場景刪除對象、部分或實例、同埋恢復一個項目歷史版本。兩者都係可撤銷、對象刪除帶一個撤銷同埋重做快照同埋恢復被記錄作為新歷史版本、所以超級確認那裏會係摩擦冇保護。對象列表刪除絕唔曾要求一個是同埋否之前。

## 設定

冇。門冇退出同埋冇持續狀態；每個打開開始自未觸及。減少運動係喺每個動畫時讀自操作系統。副本跟隨活躍語言模式同埋每個語言有趣等級通過共用 `_L()` 目錄。

## 失敗模式

- **滑塊完成而鑰匙喺同一瞬間翻轉** 、 視窗要求狀態機設定 100；如果佢拒絕、滑塊被重置同埋門保持武裝同埋取消武裝如鑰匙說。
- **錨銷毀而打開** 、 錨同埋返回焦點目標係 `wxWeakRef`；焦點恢復係跳過而唔係解引用一個死視窗。
- **停用** 、 一個錨門當佢失去激活時取消佢自己、所以佢永遠無法被留下懸掛主視窗後面同埋一個半武裝滑動。
- **減少運動** 、 `MD3::Motion::Anim::Play` 執行 `tick(1.0)` 同埋 `done()` 同步；完成突發因此完成（同埋激發回調）喺滑塊嘅完成處理器內。條紋爬行被守衛反對喺那個模式下重新啟動。
- **配置檔案輸出同埋導入**仍然使用內聯滑塊唯一門從之前呢個改變；佢被列出上方所以間隙係一個記錄決定、唔係一個疏忽。

## 安全考慮

- 門係用戶體驗連鎖、唔係授權邊界：佢保護反對滑動、唔係反對代碼直接呼叫底層操作。
- 冇關於動作、項目或計數任何是來自遠端數據；呼叫者供應佢們同埋佢們被渲染作為純文字。
- `Run()`阻止喺一個嵌套 `wxGUIEventLoop`；父視窗保持啟用（浮層語義）但門喺停用時取消、所以兩個門無法來自相同錨棧。

## 驗證

- `tests/super_confirm/super_confirm_tests.cpp`（Catch2、目標 `super_confirm_tests`）覆蓋純狀態機：未觸及、一個鑰匙只、兩個鑰匙、部分滑塊、完整滑塊、固定、喺滑動中取消武裝、取消、Escape 之前同埋授權後、鑰匙順序同埋偽造欄位。構建同埋由手喺 2026-09-08 執行：**87 assertions 喺 10 測試案例、全部通過**。
- `SuperConfirmGate.cpp`、`SlideToConfirm.cpp`同埋 `Plater.cpp` 通過 `cl /Zs` 同埋 GUI 模組嘅包括同埋定義設定。`Tab.cpp` 同埋 `GUI_Factories.cpp` 顯示只錯誤未觸及檔也顯示喺那個幫助器下（`TreeView_*` 宏、boost `CP_ACP`）、編輯區域編譯。
- 視覺行為（錨、動畫、焦點返回、螢幕讀者名稱）由構造同埋代碼審查執行；一個隱藏桌面每個門控位置嘅捕獲被記錄作為待定直到下一個完整構建。

## 建議文章

- [停止打印安全連鎖](stop-print-interlock.md) 、 原始兩鑰匙連鎖呢個門推廣。
- [鍵盤、輔助同埋回應式圖形用戶介面無障礙](gui-accessibility.md) 、 焦點、起名同埋減少運動規則門跟隨。
- [原生Material Design 3 用戶介面](md3-native-ui.md) 、 令牌、運動同埋對話框鉻。
- [英文、香港粵語同埋雙語模式](language-modes.md) 、 門嘅副本風格完全冇改變佢嘅事實。
