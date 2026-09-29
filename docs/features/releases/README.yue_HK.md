---
translation-of: README.md
source-sha256: a515b87bac10479a1bf148baf39f3b5904d68c9a68e1b114ba970cb9b6dbe614
review-status: agent-drafted
---

> 英文原文：[Release features](README.md)

# 發佈功能

- [Windows 原生安裝程式](windows-native-installer.md)
- [從原始碼構建（Windows 安裝程式）](windows-build-from-source.md)
- [單擊本機 Windows 構建同埋安裝程式](windows-one-click-build.md)
- [Windows CI 同埋發佈供應鏈](windows-release-supply-chain.md)
- [發佈代號稱](release-codenames.md)、每個發佈名稱嚟自嘅香港點心選單
- [依賴套件安全警報](dependency-security-alerts.md)：邊啲套件 manifest 會去到用家手、Dependabot 警報點樣分流，
  同埋 2026-09-29 嘅決定

呢個分支有意只發佈 Windows 安裝程式。自動上游 WinGet 同埋 Homebrew 工作被限制到上游 `bambulab/BambuStudio` 資料庫、所以分支發佈無法改變嗰啲外部軟件包源。

無適用嘅 Postman 集合：呢個類別冇暴露 HTTP API。
