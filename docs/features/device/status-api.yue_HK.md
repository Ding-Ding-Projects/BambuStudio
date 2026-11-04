---
translation-of: status-api.md
source-sha256: e504a7e43b96ccb91a69ab383b0593995c336d963e8a1211efc95ce1275da5cd
review-status: agent-drafted
---

> 英文原文：[Local printer status API](status-api.md)

# 本機打印機狀態 API

Bambu Studio 可以喺佢運行嘅電腦上面提供一個細細、唯讀嘅網頁介面，等同一部電腦上嘅其他程式（例如家居自動化腳本、直播疊加畫面、shell 提示）查詢應用程式知道邊幾部打印機，同埋某部打印機嘅門係咪開咗。預設係關閉嘅。

原生執行行為未經驗證。路由、授權同 JSON 規則由標頭層級嘅測試覆蓋；監聽器、偏好設定區同快照計時器未曾喺運行中嘅應用程式入面建置或操作過，而門讀數本身係 beta 讀數，未同真實打印機對過（見[打印機即時讀數](live-readings.md)）。

## 開啟

開啟「偏好設定」，揀 **本機狀態 API**。

| 設定 | 設定鍵 | 預設 | 備註 |
| --- | --- | --- | --- |
| 喺呢部電腦提供打印機狀態 | `status_api_enabled` | 關 | 監聽器即時開關，毋須重新啟動。 |
| 連接埠 | `status_api_port` | 18787 | 1024 至 65535 之間嘅整數。其他數值會退返 18787。如果連接埠已被佔用，會顯示「連接埠已被佔用」。 |
| 存取令牌 | `status_api_token` | 第一次開啟時產生 | 由 OpenSSL 隨機數產生器產生嘅 64 個十六進位字元。預設遮住，有「顯示」、「複製令牌」同「重新產生令牌」。 |

重新產生令牌會即時令舊令牌失效，監聽器毋須重啟。令牌同其他偏好設定一樣存放喺應用程式設定檔，永遠唔會寫入日誌，而令牌嗰行已登記到其他機密行都用嘅敏感介面登記表。

## 安全模型

- **只限本機迴路。** 監聽器只綁定 `127.0.0.1`，冇任何設定可以放寬。對端位址唔係迴路嘅連線會被丟棄，請求處理程式亦會用 403 再拒絕一次。
- **檢查 Host。** `Host` 標頭必須係 `localhost`、`127.0.0.1` 或 `[::1]`，可以帶連接埠。其他來源嘅網頁就算將自己嘅名指去 `127.0.0.1`，送出嘅 `Host` 都唔同，會收到 403。
- **一定要令牌。** 每個端點都要 `Authorization: Bearer <令牌>`，用恆定時間比對。缺少、格式錯誤或錯誤嘅令牌會得 401 同 `WWW-Authenticate: Bearer`。API 關閉或者令牌唔可用時，永遠唔會接受請求。
- **只限 GET。** 其他方法，包括 `HEAD` 同 `OPTIONS`，一律 405 同 `Allow: GET`。回應冇任何 CORS 標頭，所以瀏覽器網頁就算有令牌都讀唔到。
- **細而有上限。** 請求行同標頭合共唔可以超過 8192 位元組，唔接受請求主體，目標唔可以超過 256 個字元，五秒內未完成請求嘅連線會被關閉。每條連線只處理一個請求。
- **唯讀。** API 唔會更改任何打印機或設定。
- **純 HTTP。** 流量唔會離開呢部電腦，所以冇加密。請勿將連接埠轉發去其他機器。

檢查次序係：已啟用、對端、Host、方法、目標長度、令牌、路由。唔係嚟自本機或者唔係 GET 嘅請求，喺檢查令牌之前已經被拒絕。

## 端點

所有回應都係 UTF-8 JSON，並帶 `Cache-Control: no-store`。錯誤格式為 `{"error": "<代碼>", "message": "<文字>"}`。

### `GET /api/v1/health`

```json
{"status":"ok","api_version":1}
```

### `GET /api/v1/printers`

應用程式喺「我嘅設備」入面列出嘅打印機。

```json
{"printers":[{"dev_id":"01P00A000000000","name":"Workshop","model":"H2D","connected":true,"printing":false}]}
```

| 欄位 | 意思 |
| --- | --- |
| `dev_id` | 其他端點用嘅打印機序號。 |
| `name` | 應用程式顯示嘅名稱。 |
| `model` | 應用程式顯示嘅型號文字。 |
| `connected` | 應用程式而家同打印機有連線。 |
| `printing` | 打印機處於打印狀態。 |

### `GET /api/v1/printers/{dev_id}/door`

```json
{"dev_id":"01P00A000000000","supported":true,"state":"closed","source":"home_flag bit 23 or print.stat bit 23","beta":true,"updated_at":"2026-11-04T09:30:12Z"}
```

| 欄位 | 意思 |
| --- | --- |
| `supported` | 打印機有回報支援開門偵測（`MachineObject::support_door_open_check()`）。 |
| `state` | `open`、`closed` 或 `unknown`。打印機冇門感應器，或者應用程式未收到 `home_flag` 或 `print.stat` 嘅第 23 位元（`MachineObject::door_open_bit` 係 -1）時，會係 `unknown`，絕對唔會估。 |
| `source` | 呢個位元嘅來源。位元位置來自社群嘅協定筆記，唔係廠商文件。 |
| `beta` | 暫時一律係 `true`。 |
| `updated_at` | 應用程式最後睇到呢部打印機狀態有變（或者第一次睇到）嘅 ISO 8601 UTC 時間，唔係最後一個狀態訊息嘅時間。 |

回應：未知或者格式錯誤嘅 `dev_id` 得 `404` 同 `printer_not_found`；冇有效令牌得 `401`。

## 點樣安全咁讀取打印機狀態

打印機物件屬於 GUI 執行緒。API 開啟期間，GUI 執行緒上嘅計時器每秒將所需嘅少量欄位（編號、名稱、型號、連線、打印中、門支援同門位元）複製入一份快照。伺服器執行緒只喺互斥鎖保護下讀呢份快照，永遠唔會直接碰打印機物件，所以數值最多舊約一秒。API 關閉時快照會清空。

## 失敗情況

- 連接埠已被佔用：狀態行會顯示，而且冇任何監聽；請轉用其他連接埠。
- 冇令牌：狀態行會顯示；請用「重新產生令牌」。
- 應用程式關閉中：喺視窗銷毀之前，監聽器同計時器已經停止。
- 冇打印機：`/api/v1/printers` 回傳空清單。

## 驗證

- `node tests/status_api.test.mjs` 會編譯純標準函式庫標頭 `src/slic3r/GUI/StatusApi/StatusApiCore.hpp`，檢查路由、授權、門狀態對應同 JSON，並釘住接線（只綁迴路、要令牌、預設關閉、只限 GET）。`--negative` 會逐一移除每個保護並確認測試失敗。
- 未驗證：連同新檔案建置應用程式、喺真實 socket 上監聽、偏好設定區，以及任何真實打印機。

呢啲端點嘅 Postman 集合喺 [status-api.postman_collection.json](status-api.postman_collection.json)，亦已加入 `docs/postman/BambuStudio.postman_collection.json` 總集合。
