---
translation-of: ai-printer-watch.md
source-sha256: 0d6c135bf7c1780ba4c405d126e7f17ad26fd5dc16ef9c94c0ca29f9c634797b
review-status: agent-drafted
---

> 英文原文：[AI printer watch (local models)](ai-printer-watch.md)

# AI 打印機監視（本地模型）

**表面；** 偏好設定 ▸ 其他 ▸ *AI 打印機監視*；執行時喺`src/slic3r/GUI/PrinterWatch.{hpp,cpp}`。

## 行為

- 應用程式執行時（用戶冇必要主動監視），一個計時器（預設每 5 分鐘、`printer_watch_interval`、最少 1）捕捉設備頁面嘅 **實時相機視圖** 嘅一個幀（PrintWindow 嘅媒體控制；當冇溪流渲染時默默跳過；空白/統一幀永遠唔會產生請求）。
- 該幀（限制 768px、JPEG）去到一個 **本地** 模型通過Ollama HTTP API（`printer_watch_endpoint`、預設`http://127.0.0.1:11434`、`/api/generate`、`stream:false`）。
- 模型被詢問一個兩行判決；`OK`/`PROBLEM` 加短摘要。`OK` 變成一個安靜資訊 toast（「打印機監視；…」）；`PROBLEM`變成一個 **持續警告 toast** 描述可能發生嘅嘢（義大利麵、分離、滴水…）同埋一個具體修復建議。

## 模型

- `printer_watch_model`（預設 `qwen2.5vl`）。根據陳述偏好支援嘅族係 **gpt-oss / Qwen / Gemma**；幀需要一個視覺能力標籤（`qwen2.5vl`、`gemma3`）；純文字標籤，如`gpt-oss` 無法讀圖像，只係對文字側有用摘要。

## 隱私 & 失敗模式

- **選擇加入同埋預設關閉**（`printer_watch_enabled`）。幀永遠唔會離開機器；唯一端點係本地主機。一個請求喺飛行中一次；結果被轉運到 UI 線程。
- Ollama 冇執行 / 模型缺失 → 記錄喺資訊級別，**冇嘮叨toast** 每個間隔。冇實時視圖渲染 → 該滴係一個無操作。

## 驗證

- 編譯進 `libslic3r_gui`；捕捉路徑（視窗眨眼、統一幀跳過、JPEG/base64 限制）同埋禁用預設門衛被無頭驗證。一個端到端執行需要一個帶實時溪流嘅打印機加本地 Ollama 帶一個視覺模型；記錄為待審核硬件驗證。
