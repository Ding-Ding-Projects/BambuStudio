---
translation-of: README.md
source-sha256: 7322ba2da39498d3e999d801691a84da8f6830ea7fec18800aa42d54693507cb
review-status: agent-drafted
---

> 英文原文：[HTTP/API features](README.md)

# HTTP/API 功能

呢個類別文件由原生 Bambu Studio 應用服務嘅網絡合同。佢唔描述 Home Assistant 自己嘅 REST API 或束綁嘅 `DeviceWeb` webview。

## 合同

- [Home Assistant 打印機發現](home-assistant-printer-discovery.md) ， 一個短期、持有人認證 LAN 端點，只被啟用，當用戶啟用發現分享。

## Postman 集合

- [類別集合](postman/home-assistant-printer-discovery.postman_collection.json) ， Home Assistant 發現端點，帶合同檢查。
- [項目主集合](../../postman/BambuStudio.postman_collection.json) ， 項目級索引，包含每個 Bambu Studio 服務嘅 HTTP 合同，現已文件。

集合包含 TEST-NET 佔位符、從唔工作認證。只替換主機、端口同埋配對標記喺一個本地 Postman 環境，當一個分享視窗係活躍。標記祕密、唔保留佢到集合同埋移除環境，驗證後。

冇持久或 WAN 意圖 API 被提供。關閉智能家居對話框或禁用佢嘅分享開關無效呢個類別唯一嘅端點。
