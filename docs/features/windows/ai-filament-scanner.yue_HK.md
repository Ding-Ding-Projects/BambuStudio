---
translation-of: ai-filament-scanner.md
source-sha256: 928b8eef52c2ea5d9d4c91d03fc27b3423859bf5afaff835ac56da2112207636
review-status: agent-drafted
---

> 英文原文：[AI filament scanner](ai-filament-scanner.md)

# AI 墨水掃描器

**表面**：`File ▸ AI filament scanner…` → `FilamentScanDialog`（`src/slic3r/GUI/FilamentScanner.{hpp,cpp}`、QR 編碼供應商來自 Project Nayuki 嘅 MIT `qrcodegen` 喺 `GUI/third_party/`）。

## 流

1. 對話框開始一個 **LAN 唯一上傳服務器**（boost::beast、暫時端口）同展示一個 **QR 碼**加上平 URL。URL攜帶一個隨機每會話令牌：冇佢嘅要求得 404、上傳限制到 12 MB 同服務器同對話框死。
2. 手機打開獨立上傳頁面（雙語 EN/粵語、冇外部資產、攝像頭捕獲輸入）同發佈相片。
3. 桌面問 **本地 Ollama 視覺模型**（相同 `printer_watch_model`/`printer_watch_endpoint` 設定；預設 `qwen2.5vl`）進行嚴格 JSON：`{type, brand, color_hex, confidence}`。
4. **AMS 自動分配**：首個空 AMS 託盤配置帶認定類型 + 顏色（`command_ams_filament_settings`、溫度範圍從每材料表格）；冇打印機結果退回到「外部線軸」指導。
5. **自動打印設定**：最佳匹配墨水預設被選擇進擠出機 1：品牌映射到賣主家族實際寄運喺 `resources/profiles/BBL.json`（Bambu、SUNLU、PolyLite、PolyTerra、Overture、eSUN、Fiberon）帶 `Generic <TYPE>` 作為退回，同側欄預設刷新。
6. **公告**：一個大閃爍覆蓋（「AMS A SLOT 2」+ 墨水 + 顏色樣本；點擊/Esc/12 秒解除；閃爍跳過喺減少運動下）、一個可選 **TTS 行**（對話框度複選框、堅持、也路由到主頁助理揚聲器當配置）。AMS 硬件公開冇托盤 LED 眨眼 API，所以閃爍係螢幕上：記錄作為偏差，唔假裝。

## 隱私 & 失敗模式

- 相片永遠冇離開機器除了手機 → 桌面在 LAN；認同係本地主機 Ollama。服務器錯誤、缺失模型同格式錯誤模型回覆所有表面作為平文字；冇野阻止。

## 驗證

- 編譯入 `libslic3r_gui`；QR 從供應商編碼器渲染；服務器開始/停止同令牌門 404 路徑係無頭執行。端到端（真實手機相片 → AMS 位置）需要硬件：記錄作為待定硬件通過旁邊打印機監察。
