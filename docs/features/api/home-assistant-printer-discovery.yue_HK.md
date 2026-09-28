---
translation-of: home-assistant-printer-discovery.md
source-sha256: 9b57bede3082d25e550d90b99973b51c473bfbc3909adfa20e04a88a88f5d48d
review-status: agent-drafted
---

> 英文原文：[Home Assistant printer-discovery API](home-assistant-printer-discovery.md)

# Home Assistant 打印機發現 API

**負責人；** `HomeAssistantSharingService`
（`src/slic3r/GUI/HomeAssistantSharingService.{hpp,cpp}`）。

**呼叫者；** [Ding-Ding-Projects/ha-bambulab](https://github.com/Ding-Ding-Projects/ha-bambulab) 裏嘅 Home Assistant 發現流程。

> [!IMPORTANT]
> 呢個合約已經喺而家嘅工作樹上實現咗。佢嘅專注 Windows Release 目標、30個案例/267個斷言嘅原生套件、五個專注嘅 CTest 條目，同埋生產級別嘅跨主機發現/提取/再見探針都已經完成。完整嘅 Release GUI 構建同埋英文原生 720×760/520×480 Smart Home 剪貼檢查都完成咗。原生雙語捕捉、真實 Home Assistant 確認流程、物理打印機成功、託管 CI/release 判定，同埋遠端發佈證明已經分別記錄喺 [issue #16](https://github.com/Ding-Ding-Projects/BambuStudio/issues/16) 裏面；佢哋冇由下面嘅本地證據暗示。

## 生命週期同埋發現

呢個端點喺啟動時冇存在。佢只係喺用戶啟用 **檔案 ▸ 智能家居… ▸ 分享畀發現，五分鐘內（冇 Home Assistant 標籤）** 同埋至少有一部可訪問嘅打印機有序列號、LAN 地址同埋訪問碼時先建立。

Bambu Studio 揀咗可用嘅 RFC1918 或共享地址空間 IPv4 介面喺普通預設路線上，並將 TCP 綁定到嗰個地址上嘅操作系統揀咗嘅非特權端口。

預設路線檢查係有意做嘅；一個僅主機 Hyper-V、WSL 或容器適配器可能擁有內核嘅偏好多播路由，但仲然喺物理 LAN 到達唔到。廣播嗰個虛擬地址會令本地測試通過，但會阻止另一部電腦上嘅 Home Assistant 提取要約。

呢個服務會解析揀咗嘅介面嘅真實前綴長度。佢只係答複可用嘅私有/共享發送者地址喺嗰個確切連結上嘅 mDNS，並且喺構建回應前拒絕網絡、廣播、公開、迴環同埋連結本地來源。

佢廣播；

| DNS-SD 項目 | 值 |
| --- | --- |
| 服務類型 | `_bambu-slicer._tcp.local.` |
| PTR | 一個全新嘅每視窗服務實例 |
| SRV | 生成嘅 `.local` 主機同埋揀咗嘅 TCP 端口 |
| TXT `pairing` | 呢個分享視窗嘅全新 URL 安全承載能力 |
| TXT `name` | 通用 `Bambu Studio`（Windows 主機名冇被發佈） |
| A | 揀咗嘅 LAN IPv4 地址 |
| 普通 TTL | 120 秒 |

呢個應答器答複佢嘅記錄嘅 PTR、SRV、TXT、A 同埋 ANY 問題。佢仲會喺分享保持活躍時發送未經請求嘅公告。關閉分享、關閉對話框或到達五分鐘自動過期會發送相同嘅記錄並將 TTL 設為零、關閉 HTTP 會話，同埋摧毀呢個能力。

mDNS 允許使用一個全局突發八個查詢，每秒補充一個標籤，然後先分配回應。回應通過一個飛行中嘅發送、最多八個回應嘅隊列、端點加事務 ID 合併，同埋 50 毫秒間隔進行。多播查詢洪泛因此無法建立一個無限嘅分配或非同步發送鏈。停止同埋重新啟動會取消節奏計時器並丟棄舊隊列。

## 請求

```http
GET /bambustudio/printers HTTP/1.1
Host: <advertised-ip>:<advertised-port>
Authorization: Bearer <pairing TXT value>
```

要求；

- 方法恰好係 `GET`；
- 目標恰好係 `/bambustudio/printers`；
- 恰好存在一個 `Authorization` 標頭；
- 佢嘅方案同埋值恰好係 `Bearer <pairing-token>`；同埋
- 呢個請求冇主體。

承載者比較係恒定時間。認證發生喺 Bambu Studio 向打印機數據供應器請求其有效負載前，所以未經認證嘅請求無法呼叫嗰個供應器或接收打印機數據。

## 成功回應

```http
HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8
Cache-Control: no-store
Connection: close
X-Content-Type-Options: nosniff
```

```json
{
  "printers": [
    {
      "serial": "01P00A000000000",
      "host": "192.0.2.44",
      "access_code": "replace-with-a-local-test-value",
      "name": "Optional display name"
    }
  ]
}
```

`serial`、`host` 同埋 `access_code` 係必需嘅非空字串。`name` 係可選嘅。內部提供嘅未知字段會喺回應前被移除。

## 限制同埋狀態碼

| 合約 | 限制或結果 |
| --- | --- |
| 請求標頭塊 | 8 KiB |
| 請求目標 | 256 字節 |
| 請求超時 | 5 秒 |
| 並發會話 | 16 |
| 授權回應速率 | 突發 4，每秒補充 1 個請求 |
| 回應主體 | 64 KiB |
| 打印機計數 | 前 32 個條目 |
| 每個打印機字段 | 256 字節 |
| 格式錯誤嘅 HTTP | `400 Bad Request` |
| 缺失、重複或無效嘅授權 | `401 Unauthorized`，帶 `WWW-Authenticate: Bearer` |
| 授權速率限制已耗盡 | `429 Too Many Requests`，帶 `Retry-After: 1` |
| 錯誤嘅路徑 | `404 Not Found` |
| 錯誤嘅方法 | `405 Method Not Allowed`，帶 `Allow: GET` |
| 缺失、格式錯誤或超出限制嘅提供打印機數據 | `503 Service Unavailable` |

每個回應都帶有 `Cache-Control: no-store`、`Connection: close` 同埋 `X-Content-Type-Options: nosniff`。錯誤主體係固定文字，永遠唔會回顯請求、配對標籤、供應器異常或打印機數據。

回應預算喺供應器執行前進行檢查。第一個有效嘅淨化有效負載會被快取於分享視窗嘅生命週期內，所以後續授權嘅請求會重複使用嗰個有界快照，而唔係重複詢問 UI 側供應器序列化憑據。無效嘅供應器輸出永遠唔會被快取。

## 安全邊界

呢個係一個能力保護嘅清文 LAN 端點，唔係 HTTPS。配對標籤必須可被 Home Assistant 發現，所以佢存在於 mDNS TXT 記錄中，並且喺相同廣播域上嘅其他設備可見。任何觀察到佢嘅人都可以喺用戶禁用分享或五分鐘計時器過期前，唔使 Home Assistant 嘅確認螢幕就提取打印機有效負載。打印機訪問碼可以控制 LAN 上嘅打印機。

因此；

- 只喺信任嘅 LAN 上啟用分享，並且只啟用最短實用時間；
- 確認預期嘅 Home Assistant 發現卡片，然後關閉分享而唔係等待五分鐘過期；
- 唔好通過路由器轉發端口、代理佢去另一個網絡或將佢曝露到 WAN；
- 唔好喺 Postman 集合、環境、日誌、截圖、問題或原始碼控制中儲存配對標籤或回應主體；同埋
- 將意外嘅發現請求視為停止分享同埋調查 LAN 嘅原因。

呢個服務只記錄通用嘅生命週期/解析器失敗。佢有意永遠唔會記錄配對標籤、請求 Authorization 標頭、打印機有效負載、訪問碼或供應器異常詳情。

## Postman

導入以下任一者；

- 專注嘅 [類別集合](postman/home-assistant-printer-discovery.postman_collection.json)；或
- [Bambu Studio 主集合](../../postman/BambuStudio.postman_collection.json)。

保持集合嘅 TEST-NET 預設值。喺一個本地、一次性 Postman 環境中，從活躍 mDNS 記錄設定 `share_host`、`share_port` 同埋 `pairing_token`。授權嘅請求檢查回應狀態、no-store 標頭、JSON 形狀、計數限制、必需字串字段，同埋每字段大小限制，唔使打印回應數據。

Postman 只係一個合約檢查輔助工具。完成嘅跨主機傳輸通過使用生產 C++ 探針；最後接受仲需要一個真實 Home Assistant 發現流程，因為手動提供嘅 Postman 標籤唔能證明確認卡片。

## 驗證矩陣

Windows SDK 10.0.26100.0 Release 目標已構建並喺 2026-07-28 執行。
`home_assistant_tests` 通過咗 **30 個測試案例 / 267 個斷言**，專注嘅 CTest 選擇通過咗 **5/5** 條目。涵蓋包括；

- HTTPS、迴環 HTTP、清文 LAN HTTP、格式錯誤嘅 URL 同埋不支援嘅方案嘅 URL 策略；
- 有界嘅工作線程取消、一次性 UI 完成、異常遏制同埋關閉回調；
- 有界實體域驗證、4 MiB 狀態主體傳輸、512 後端結果、256 渲染行同埋持久化列表掃描預算；
- 生成安全光快照、批處理/上限、超越、關閉恢復、保留恢復場景當兩個恢復嘗試都失敗，同埋喺取消登陸後但場景建立前刪除；
- 四路路徑 B 打印機導入波，將 32 個無法訪問嘅打印機限制到最多八個 30 秒超時波，而唔係序列等待；
- 全新 URL 安全配對標籤；
- 穩定結構化分享啟動失敗；
- 拒絕迴環/公開廣播地址同埋非連結、網絡、廣播、公開同埋連結本地 mDNS 發送者；
- 授權前缺失、錯誤同埋重複嘅授權拒絕；
- 授權規範有效負載同埋 no-store 標頭；
- 精確方法同埋目標行為；
- 格式錯誤、超大同埋超計數有效負載處理；
- 已認證標籤桶補充、供應器快取同埋無效供應器預算消耗；
- 一個 512 查詢 mDNS 節奏/合併洪泛加上全球有界預分配允許；
- 預設路線介面選擇同埋真實介面前綴查找而唔係一個僅主機多播優先適配器；同埋
- 冪等重啟加序列化並發停止/連接清理。

完整嘅 `BambuStudio_app_gui` Release 目標仲喺 3,387 秒後以 0 退出，隨後 8.3 秒無變化重構。原生剪貼檢查曝露並修復咗一個文字操作大小缺陷；專注嘅 `SmartHomeDialog.cpp` 重構/連結以 141 秒退出 0，其無變化構建以 8.0 秒。後來嘅非視覺導入調度同埋取消清理更改編譯同埋連結以 214.808 秒，隨後 8.544 秒無變化構建。最後 151,299,584 字節 DLL 帶時間戳 `2026-07-28 08:15:46 -04:00`，SHA-256
`41BB1BFC754E3184C5908E2145A93E3640D3866E59380F32EEFF7A76F418E972`。英文 720×760 同埋 520×480 更正捕捉從嗰個確切最後 DLL 重新捕捉，並且喺 [`docs/screenshots/smart-home`](../../screenshots/smart-home/) 下索引；原生雙語捕捉仲待審核。

`home_assistant_sharing_probe` 提供一個合成 TEST-NET 打印機，只用於跨主機傳輸驗證。第二個 LAN 主機觀察咗 PTR/SRV/TXT/A 記錄、完成咗一個授權有界提取而冇打印配對值或有效負載，同埋觀察咗零 TTL 再見。佢嘅結果唔應該被描述為成功嘅真實打印機導入或真實 Home Assistant 確認卡片通過。
