---
translation-of: live-notifications-atlas.md
source-sha256: 15affaf9857f295c131e791c7cca42d4338c59c822ad4f3929c1b0af84097a72
review-status: agent-drafted
---

> 英文原文：[Studio Atlas live notification presentation](live-notifications-atlas.md)

# Studio Atlas 即時通知外觀

純外觀單元按 `design/workflow-refresh/surface-contracts.json` 更新渲染器通知卡，基準 `35a329da565a61b98f541166e51b85c44958add8`。`NotificationCenterPanel` 同通知歷史不變。

## 組合範圍

保留反向表面卡同原有量度字型，將整塊全高彩色左帶改成文字縮排範圍內嘅幼身狀態條，由圓角以下開始。淡反向內容邊線界定卡片，唔增加版面邊框。警告、錯誤同普通強調色仍由原邏輯決定。

圓角喺通知存續時讀取目前舒適／緊湊密度同畫布比例。可見關閉、最小化、取消上載控件有半透明反向內容懸停／按下層；放大嘅隱形操作區保持透明，避免蓋住文字同圓角。標籤、圖示、位置、點擊範圍、字號、寬度同換行規則不變。

進度軌道配合反向卡片使用半透明反向內容色，原強調填色、端點、百分比同文字計算保留。淡出用原透明度，冇新增動畫、排程或畫面請求。

`NotificationManager.cpp` 只改以下方法：

- `PopNotification::ensure_ui_inited`：更新裝飾圓角。
- `PopNotification::bbl_render_left_sign`：狀態條同內縮邊線。
- `PopNotification::render_close_button`：可見狀態層、透明放大目標。
- `PopNotification::render_minimize_button`：可見狀態層。
- `PrintHostUploadNotification::render_cancel_button`：可見狀態層、透明放大目標。
- `ProgressBarNotification::render_bar`：只改軌道顏色同 alpha。

飽和彩色阻斷警告／錯誤橫額不變。圖示可用性同訊息語意保留；冇宣稱新增狀態文字、完整語意圖示或完整通知功能。

## 保留同驗證

`node --test ui-md3/tests/notification-bilingual-links.test.mjs ui-md3/tests/preview-overlays.test.mjs` 共八項通過。刻意將量度後雙語操作標籤換成原始標籤，已觀察檢查拒絕；恢復原位元組後完整有限命令再次通過。冇新增只比對繪畫數值嘅測試。

同基準比較，正規化換行後十一個方法內容不變：`PopNotification::render`、`bbl_render_block_notification`、`fit_to_stack`、`count_spaces`、`count_lines`、`set_next_window_size`、`render_text`、`on_text_click`、`on_second_text_click`、`update_state` 同 `NotificationManager::render_notifications`。保留堆疊／停靠、文字量度、生命週期、逾時同派發。三個改動嘅操作繪畫方法保留原目標計算同回呼語句。

冇完整建置、啟動、擷圖、硬件、安裝、發佈或部署。原生編譯及真實通知卡仍須驗證所有語言、主題、密度、比例、懸停／按下／停用、捲動、重疊、減少動態、自訂種子色對比、進度同取消。來源檢查唔能夠證明呢啲執行結果。

## 還原邊界

單元只包括 `NotificationManager.cpp` 外觀同文章，可獨立還原，保留較早渲染／提示修正及通知歷史、生命週期、傳輸、取消同排版工作。唔應重設倉庫或刪除通知功能。
