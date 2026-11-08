---
translation-of: attention-modes.md
source-sha256: b8b4b92f51a135854ece692fecd7920fd5012031de981b6adebdddad898a8a53
review-status: agent-drafted
---

> 英文原文：[Attention modes](attention-modes.md)

# 專注模式

**表面**：偏好設定 > 專注模式、標題列下面嘅橫條、指令面板（`Ctrl+Shift+F`，打「Attention」）同全部功能頁 → `src/slic3r/GUI/AttentionModes/AttentionService.{hpp,cpp}`。規則喺冇 wxWidgets 嘅 `AttentionModel` 度（`src/slic3r/GUI/AttentionModes/AttentionModel.{hpp,cpp}`）。

五個介面上嘅遷就。每一個**預設都係關**，每一個都可以獨立開關：冇一個需要另一個，亦冇一個開關可以一次過開晒。佢哋只會改變 Bambu Studio 喺你面前嘅樣同表現。冇嘢會被量度、評分、計連續日數或者分享，亦唔係任何建議或者對任何人嘅判斷。

| 模式 | 做乜 |
| --- | --- |
| 專注 | 喺準備揀咗嘢（而且冇工具例如上色喺度做緊嘢）嘅時候，打印板上其他嘢會畫得淡啲、偏灰。乜都唔會收埋：取消揀選，或者喺橫條撳**關閉專注**，所有嘢就返晒嚟 |
| 低刺激 | 少啲動態：過場即刻完成，同減少動態設定一樣，只會再加，永遠唔會取消嗰個設定。冇動作音效。開機冇點心驚喜 |
| 時間感 | 標題列下面嘅橫條會講今次開咗幾耐，同埋上次改嘢係幾耐之前，例如「已開 1 小時 12 分鐘 · 上次改動 4 分鐘前」，或者「仲未有改動」 |
| 一次一件事 | 橫條會顯示你打嘅下一步（「下一步：檢查打印板 2 嘅邊緣」），有**編輯**同**完成**。重開都會保留，直到你標記完成 |
| 動力 | 一段設定嘅時間（15 或 30 分鐘、1 個鐘或者 1 個半鐘）冇改動，就會有一個安靜嘅通知照直講，例如「呢度 40 分鐘冇改動。」每段冇郁過嘅時間只會講一次，**唔係而家**會喺你設定嘅時間（30 分鐘至 4 個鐘）保持安靜，重開都算 |

改動即係任何會加一步復原嘅嘢：移動、加入或者移除物件、改物件嘅設定等等。

## 橫條

標題列下面嘅橫條只會喺時間感、一次一件事或者專注開咗嘅時候出現。佢只顯示嗰啲模式需要嘅嘢，每兩秒更新一次（時間以完整分鐘計），佢嘅**調校**按鈕會打開偏好設定嘅專注模式。

## 學校模式

學校模式唔會郁專注模式：佢哋係遷就，唔係外觀選擇，所以學校嘅設定都可以好似其他人咁開或者關佢哋。

## 儲存嘅值

| AppConfig 鍵 | 值 |
| --- | --- |
| `attention_focus`、`attention_low_stimulation`、`attention_time_awareness`、`attention_one_thing`、`attention_momentum` | `true` 或者 `false`；除非你開咗，否則係 `false` |
| `attention_momentum_minutes` | `15`、`30`、`60` 或者 `90` |
| `attention_not_now_minutes` | `30`、`60`、`120` 或者 `240` |
| `attention_next_action` | 下一步，一行，最多 120 個字元 |
| `attention_snoozed_until` | **唔係而家**幾時完，以 1970 年起計嘅秒數 |

## 檢查

`tests/attention_model.test.mjs` 會編譯 `tests/attention_model_test.cpp`：每個模式預設關而且互相獨立、儲存同讀取、下一步保持一行、經過嘅時間，同埋動力每段時間只講一次、喺唔係而家期間永遠唔講。佢嘅反向建置會令動力重複講，必須失敗。

## 未做

準備檢視以外嘅專注（清單同偏好設定嘅行）同低刺激嘅較安靜配色仲未起。
