---
translation-of: scheduled-settings.md
source-sha256: f90a381c169edf830c0dec7a2d1b54430678aa57c20cccef58c9e7f94a33ae52
review-status: agent-drafted
---

# 排程設定

Preferences > Schedules 容許規則喺選定時間、透過版本化 HTTPS 設定 API，或者 Home Assistant 開關，改變語言模式、主題、密度、強調色、字體、文字大小、兩個趣味程度同程式顯示名稱。規則唔再符合時，會恢復自己設定嘅值。每次規則編輯都係普通偏好儲存，所以偏好歷史會好似其他設定變更一樣記錄，亦可以還原較早排程。

程式碼：`src/slic3r/GUI/Schedule/ScheduledSettingsModel.hpp`（唔依賴 wx 嘅結構描述、匹配器、優先次序、API 合約同覆寫記錄）、`ScheduledSettings.{hpp,cpp}`（計時器、來源同套用值）、`ScheduledSettingsPanel.{hpp,cpp}`（偏好設定區同規則編輯器）。交付狀態：由已保留實作恢復並配合目前介面調整。呢次超快速交付冇執行測試、lint、執行時啟動或者畫面擷取。

## 行為

- 程式啟動時、之後每 60 秒，以及編輯規則或者通知 **Retry** 操作後立即評估。Schedules 顯示上次檢查時間。
- 規則啟用、本機時間喺時間段內、當日係所選星期，而且日期喺可選日期界限內時符合。
- **優先次序：** 按清單次序評估；多個規則設定同一值時，較後規則勝出。向下移動可提高優先。清單本身就係優先次序，冇另一優先欄位可能偏離。
- **基礎設定：** 規則第一次接管設定時會保存之前值。冇規則再接管就寫返原值。記錄存喺排程旁邊（`BambuStudio.conf` 嘅 `scheduled_settings_override`），所以程式關閉期間結束嘅規則，亦會喺下次啟動還原。遠端同排程值永不寫入呢個記錄。
- 規則持有設定期間，喺 Appearance 或 General 改值會喺下次檢查被覆寫。應停用或者編輯規則。
- 空排程或者冇符合規則唔改任何值。

## 結構描述（`scheduled_settings`，schemaVersion 1）

```json
{
  "schemaVersion": 1,
  "rules": [
    {
      "id": "r-1a2b3c4d",
      "label": "Evening dark mode",
      "enabled": true,
      "startDate": "2026-09-01",
      "endDate": "2026-12-31",
      "startTime": "20:00",
      "endTime": "07:00",
      "weekdays": ["mon", "tue", "wed", "thu", "fri"],
      "source": { "kind": "local" },
      "values": { "dark_color_mode": "1", "ui_density": "compact" }
    }
  ]
}
```

- `id` 穩定，永不重新編號；`weekdays` 係 `"everyday"` 或 `mon`..`sun` 清單。
- `source.kind` 係 `local`、`api`（`url`、`allowLoopbackHttp`）或者 `homeAssistant`（`entityId`）。
- `values` 只容許下面白名單鍵。讀取時丟棄其他鍵，所以手改文件唔可以排程 token、路徑或者憑證。
- 文件同每條規則嘅未知欄位會喺來回儲存保留，舊建置儲存時唔會丟失新建置資料。
- 如果文件 `schemaVersion` 未知或者剖析失敗，磁碟原件保持不變，排程器以冇規則運行。

容許鍵同接受值：

| 鍵 | 設定 | 接受值 |
| --- | --- | --- |
| `language` | 語言模式 | `en_US`、`yue_HK`、`bilingual_en_yue_HK`（任何由字母、數字、`_`、`-` 組成嘅 id） |
| `dark_color_mode` | 主題 | `0` 淺色、`1` 深色 |
| `ui_density` | 密度 | `comfortable`、`compact` |
| `ui_accent_seed` | 強調色 | `#rrggbb`，留空使用品牌 seed |
| `ui_font_family` | 字體 | 已安裝字體名稱，留空使用預設 |
| `ui_font_scale` | 文字大小 | 0.8 至 1.4 數字（選擇器提供 0.9 / 1.0 / 1.15） |
| `funny_level_en` | 英文趣味程度 | `1` 至 `5` |
| `funny_level_yue` | 粵語趣味程度 | `1` 至 `5` |
| `app_display_name` | 程式名稱 | 最多 40 字元，冇控制字元；留空重設 |

## 時間語意

全部時間使用 C runtime 報告嘅電腦本機時鐘。設定區同編輯器顯示目前偏移同夏令時間是否生效。

- `startTime < endTime`：所選星期嘅時間段為 `[start, end)`。
- `startTime > endTime`：跨午夜，由所選星期開始至翌朝；星期同日期界限按時間段**開始**嗰日判斷。所以星期五 22:00-06:00 包含星期六 03:00，但唔包含星期日 03:00。
- `startTime == endTime`：所選星期全日。
- 日期上下界都包含當日，兩端都可省略。
- **夏令時間變更**移動本機時鐘。跨越變更嘅時間段當晚會短或者長一小時。當晚不存在嘅開始分鐘（跳過嗰小時）視為已過；重複出現嘅結束分鐘喺首次出現就結束。冇用 UTC 排程。
- 冇選星期嘅規則永不符合，編輯器拒絕儲存。

## 來源

**Local** 規則喺整段時間套用自己嘅值。

**HTTPS API** 規則喺時間段內每次檢查讀取 `GET <url>` 並套用回應：

```json
{ "schemaVersion": 1, "values": { "dark_color_mode": "1", "ui_density": "compact" } }
```

- 只接受 `https://`，例外係規則啟用 *Allow plain http:// to localhost* 時，容許 `http://` 連到 `localhost` / `127.x` / `::1`。帶使用者名稱或者密碼嘅位址會拒絕。唔跟隨重新導向，3xx 視為失敗。
- 回應限 64 KiB 同 10 秒；必須係 JSON 物件，帶 `schemaVersion: 1` 同 `values` 物件。白名單外鍵忽略；類型錯誤或者超接受範圍嘅值會拒絕整個回應。
- API 冇提供嘅鍵，以規則自己勾選值作後備；有提供嘅鍵由 API 優先。
- 每次請求帶世代編號。舊請求答案會丟棄，慢回覆永不覆蓋新回覆。

**Home Assistant switch** 規則透過 Smart home 已設定連線，讀取一個實體（`input_boolean.*` 或 `binary_sensor.*`）。現有有界 `GET /api/states` 查詢喺工作器按請求實體 domain 篩選，排程器再選精確實體 id。遭截短查詢缺少實體時視為不可用。`on` 套用規則勾選值；`off` 釋放。使用同一世代保護。

未回覆、回覆錯誤，或者唔係 `on`／`off` 嘅來源唔提供任何值，所以不穩定伺服器唔會自行反覆切換設定。

## 後備同通知

- 連續失敗中第一次會發非阻塞警告通知，列出規則同原因（HTTP 狀態、拒絕重新導向、拒絕結構描述、缺少 Home Assistant token、主機不可達），帶 **Retry**。同一連續失敗嘅後續只更新規則狀態列。
- 來源失敗或者狀態未知時，該規則唔提供值，相關設定恢復已保存基礎值，除非另一符合規則持有。
- 遠端值永不持久化為使用者基礎值。停用規則或者時間段結束會恢復已保存基礎。
- Schedules 逐規則顯示是否喺時間段內、來源最後回覆，同目前控制哪些設定。

## 安全

- 白名單優先：模型喺任何儲存或者套用之前，丟棄唔屬於上面九個嘅鍵，文件同 API 回應都一樣。
- URL 政策：只用 HTTPS；loopback HTTP 須明確逐規則旗標；URL 冇憑證；唔重新導向；尺寸同逾時有界。
- Home Assistant 使用現有客戶端：token 留喺 `BambuStudio.conf`，只經 HTTPS 或 loopback 位址發送；查詢 domain 前將實體 id 驗證為 `<domain>.<object_id>`。
- 評估器只寫九個白名單鍵。排程、API 答案或者 Home Assistant 狀態唔能到達路徑、token 或打印機憑證。
- 除 API `GET` 同 Home Assistant `GET`，排程資料唔離開電腦；兩個請求都唔攜帶設定。

## 與其他功能互動

- 語言模式：新目錄供之後建立嘅視窗載入；運行中框架保留字串至下次啟動，與 General 語言選擇器說明一致。
- 主題、密度、強調色、字體同文字大小使用與 Appearance 相同即時傳播。
- 趣味程度同程式名稱即時套用。
- 偏好歷史每次儲存都快照排程；還原較早快照就取回較早排程。

## 交付證據同剩餘驗證

已保留源碼經選擇性恢復，冇包括無關 Home Assistant 設定同步功能或者其生命週期掛鈎。Schedules 已接入 Preferences，排程器喺偏好歷史設定之後啟動，關閉時喺 Home Assistant 清理之前令待處理回覆失效。

呢次超快速交付刻意冇跑測試、lint、執行時互動、安裝程式執行或者畫面擷取。建置同封裝結果屬於發行協調者固定候選。本文唔聲稱已恢復介面通過執行時驗證。

## 廣東話使用說明

喺偏好設定打開「Schedules」，新增或編輯規則，揀日期、星期同開始及結束時間。
結束時間早過開始時間代表跨午夜；兩個時間相同代表全日。清單較後嘅規則有較高優先權。
規則可以用本機設定、HTTPS 設定 API，或者 Smart home 入面已連接嘅 Home Assistant 開關。
來源未能回覆或狀態唔明確時，該規則唔會提供設定值；冇其他規則接管嘅設定會還原。
關閉規則或時間段結束時，原本設定會返嚟。設定同還原記錄會保存在本機設定檔。
今次快速交付冇跑測試、冇啟動介面、冇截圖；唔應該將原始碼恢復當成已驗證運行。
