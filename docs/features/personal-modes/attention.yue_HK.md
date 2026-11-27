---
translation-of: attention.md
source-sha256: 916ea53419643ba7fa37984d6a0c9bd287fa74c17d62c1ceb061a81575f77a49
review-status: agent-drafted
---

# 專注輔助

`AttentionModes` 提供五項獨立設定，預設全部關閉。應用程式負責儲存 `AttentionSettings`，並透過 `AttentionCallbacks` 接駁實際功能。如果儲存唔成功，繼續沿用之前嘅設定。呢啲功能唔提供診斷、評分或者療效聲稱。

| 輔助功能 | 回呼同工作區行為 |
| --- | --- |
| 專注 | `focus(bool)` 調暗周圍內容，突出目前目標，其他內容仍然用得。 |
| 低刺激 | `low_stimulation(bool)` 減少動態效果，畫面變得柔和。`suppress_nonessential_notifications()` 只會過濾非必要通知。 |
| 時間提示 | `elapsed(visible, session, idle)` 喺工作位置顯示今次使用時間同閒置時間。 |
| 一次一件事 | `next_action(visible, text)` 顯示使用者自己揀好並儲存嘅下一步，唔會推測工作。 |
| 動力提示 | `momentum(visible, idle)` 喺一段時間冇活動時顯示可以關閉嘅提示。`dismiss_momentum()` 跟從設定嘅延後時間。 |

使用者實際編輯或者轉頁時，呼叫 `activity()`；每秒呼叫一次 `tick()`。計時用單調時鐘。預設閒置時間係 20 分鐘，延後時間係 30 分鐘，兩者都可以設為 1 至 240 分鐘。每段閒置期間只會提示一次，除非明確設定嘅延後時間已過。使用者有活動就會收起提示，重新計算閒置時間。下一步嘅文字上限係 4096 個 UTF-8 位元組。介面要喺儲存前解釋限制並驗證輸入。

作業系統嘅減少動態效果偏好有變時，呼叫 `platform_reduced_motion()`。關閉低刺激唔會取消系統偏好，所以回呼收到嘅減少動態效果值已包括系統選擇。應用程式開關本身嘅配色同通知處理，請查看 `settings().low_stimulation`。學校同兒童顯示模式亦要保留呢五項獨立輔助功能，整合時要明確接駁。

設定、搜尋同指令面板項目都要用應用程式共用嘅本地化控制元件，每個搜尋欄旁邊都有獨立嘅進階正規表達式編輯器。服務本身唔會新增搜尋欄或者設定頁。
