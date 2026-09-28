---
translation-of: non-blocking-notifications.md
source-sha256: 6492a29e9830f4f49371219e208172a791423a877370bbea5b77ea061c069a0b
review-status: agent-drafted
---

> 英文原文：[Non-blocking notifications](non-blocking-notifications.md)

# 非阻塞通知

資訊、成功、警告同埋錯誤消息，不需要決定被呈現為非阻塞角落 toast（存在喺畫布`NotificationManager` snackbar），唔係模態對話框。模態對話框保持保留用於真實決定；確認、未儲存更改提示、破壞性操作門衛同埋憑據步驟。

## 行為

喺 `src/slic3r/GUI/GUI.cpp` 中三個中央消息漏斗路由到角落toast；

| 漏斗 | 級別 | Toast 行為 |
| --- | --- | --- |
| `show_info(parent, message, title)` | 定期 | 褪去（~10 秒） |
| `warning_catcher(parent, message)` | 警告 | 保持，直到被駁回 |
| `show_error(parent, message)` | 錯誤 | 保持，直到被駁回；最上面 |

呢樣涵蓋約 120 個 OK 只呼叫位置應用程式範圍內冇觸及每個呼叫者。Toast 喺 GL 畫布（右下角區域）渲染、堆積冇重疊，同埋支援管理器嘅超連結/操作承諾。

### 模態後備

`try_push_corner_notification` 後備到原始模態對話框當一個toast 無法被看到時；

- 喺 Plater / NotificationManager 存在前（早期啟動，例如配置嚮導失敗），或
- 當另一個模態對話框係頂部時（toast 畫布會被覆蓋；通過掃描 `wxTopLevelWindows` 檢測為活躍 `wxDialog::IsModal()`）。

冇消息永遠被默默丟棄；後備顯示呢個確切對話框呼叫位置使用前呢個特性。

## 配置

冇。路由係無條件嘅；通知歷史保持可檢查通過通知管理器嘅堆積列表喺畫布上。

## 失敗模式

- **模態喺頂部** → 模態後備（按設計，睇上面）。
- **消息從工作線程發佈**；`show_error` 已經通過`CallAfter` 進行轉運；`show_info`/`warning_catcher` 保持佢哋嘅原始線程期望（主線程），從前冇改變。
- **GL 畫布不可用**（無頭/初始化失敗）→ Plater 檢查失敗 → 模態後備。

## 安全考量

消息文字由 ImGui 作為純文字渲染；超連結動作只係那些呼叫位置明確連接嘅。冇消息內容被解釋為標記或執行。

## 驗證

- 構建乾淨進 `libslic3r_gui`（0 個錯誤）同埋運送喺提交`ab007cda2` 中。
- 無頭視覺證明；喺截圖矩陣下捕捉喺`docs/screenshots/notifications/`（toast 渲染喺 Mesa GL 畫布通過PrintWindow）。

## 建議文章

- [通知中心](notification-center.md)；頂部欄上嘅鐘保持每個 toast 嘅可搜尋、可匯出歷史記錄喺呢度記錄，所以一個褪去消息永遠唔會丟失。
