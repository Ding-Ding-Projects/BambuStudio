---
translation-of: README.md
source-sha256: 9e561d8b57c30300df5a4d195d6786f8193f6c3e964ced2e4158f75156923791
review-status: agent-drafted
---

> 英文原文：[Device features](README.md)

# 設備功能

- [LAN 打印機場](farm-lan.md)：設備庫存、選擇、本地發送同任務結果。
- [即時圖表](live-charts.md)：照打印機送嚟嘅樣，顯示噴嘴、熱床同機箱溫度隨時間嘅變化，有目標線、蠟燭圖、鍵盤十字線同 CSV。
- [門鈴](door-chime.md)：打印機門打開嘅時候播一段鈴聲，關上嘅時候播另一段，喺 Device 頁揀同預覽。
- [打印機旋律](printer-melody.md)：編寫打印機用 M1006 播放嘅旋律，預覽、喺打印機播放，或者加入開始或者結束 G-code。
- [打印機即時讀數同打印機選項](live-readings.md)：打印監控上嘅 Wi-Fi、風道、開門偵測設定同噴嘴晶片，以及打印選項同安全選項入口幾時出現。
- [本機打印機狀態 API](status-api.md)：預設關閉、只限本機迴路、唯讀、要持有人令牌嘅網頁介面，提供打印機清單同 beta 門狀態（[Postman 集合](status-api.postman_collection.json)）。

本機打印機狀態 API 係呢個類別唯一嘅 HTTP API；其他文章講嘅功能都冇 HTTP API。
