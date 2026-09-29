---
translation-of: README.md
source-sha256: f5e93614d1b656060f444db8d96492cf869829ddea6b8d171ea066135d622ec2
review-status: agent-drafted
---

> 英文原文：[Windows features](README.md)

# Windows 功能

- [Windows 專用平台政策](windows-only-platform.md)
- [原生 Material Design 3 UI](md3-native-ui.md)
- [鍵盤、輔助同回應 GUI 可訪問性](gui-accessibility.md)
- [英文、香港廣東話同雙語模式](language-modes.md)
- [有趣等級同對話框 emoji](funny-levels-and-dialog-emojis.md)
- [墨水術語（墨水 → 墨水、AMS → 墨水機）](ink-terminology.md)
- [外觀自訂](appearance-customization.md)
- [可重新命名應用名稱（顯示標籤只）](renamable-app-name.md)
- [每元素外觀編輯器（右鍵點擊 ▸ 編輯外觀...、Ctrl+Shift+E）](appearance-editor.md)
- [正則表達式建立者](regex-builder.md)
- [Material 內容選單（搜尋、快速鍵、子選單）](material-context-menus.md)
- [命令調板（Ctrl+Shift+F）](command-palette.md)
- [應用內變更日誌查看器（幫助 ▸ 有咩新嘢）](changelog-viewer.md)
- [準備側邊欄搜尋（設定 + 墨水插槽）](sidebar-search.md)
- [Material 顏色選擇器 & 顏色翻譯器](md3-color-picker.md)
- [顏色選擇器翻譯器（無限選擇器、每個符號、色域、對比）](color-picker-translator.md)
- [批量墨水動作](bulk-filament-actions.md)
- [停止打印安全互鎖](stop-print-interlock.md)
- [破壞性動作超級確認（兩鍵 + 完整滑動）](super-confirmation.md)
- [打印模擬播放（進給速率真實）](print-simulation.md)
- [板打印動作同材料對應](print-actions.md)
- [LAN 場發送同設備資格](lan-farm-sending.md)
- [AI 打印機監察（本地模型）](ai-printer-watch.md)
- [AI 墨水掃描器（QR 手機上傳 → AMS 插槽）](ai-filament-scanner.md)
- [智能家庭：打印機移交、TTS 敘述者同警報燈](smart-home.md)
- [發佈啟動畫面藝術（每個發佈新鮮點心）](release-splash-art.md)
- [啟動畫面上嘅發佈日期](splash-release-date.md)
- [點心啟動驚喜（十分之一啟動）](dim-sum-surprise.md)
- [原生視覺煙霧測試](native-visual-smoke.md)
- [雲網頁故障恢復](cloud-web-recovery.md)
- [軟件 OpenGL 後備（Mesa llvmpipe）](software-gl-fallback.md)
- [應用更新從呢個分支發佈](app-updates.md)

Windows 係呢個分支嘅活躍發佈目標。macOS 同 Linux 源支援保持上游，但呢啲平台唔係分支發佈接受門嘅部分。

應用依家暴露一個緊密範圍 HTTP 合約只當用戶明確啟用 Home Assistant 打印機發現時。佢嘅端點、安全邊界同 Postman 集合記錄喺 [HTTP/API 功能](../api/README.md) 下面。佢係一個短生命認證移交，唔係一般遠端控制 API。

`DeviceWeb` 次項目（`src/slic3r/GUI/DeviceWeb/`）保持一個應用內 webview 前端束縛帶著應用，唔係一個提供 HTTP API；呢個資源庫發佈冇獨立 API 合約用於佢。

## 官方源重構

- [源庫存同重新應用](../../reapplication/README.md)
- [項目載入適配器同需要執行時情況](../../reapplication/loader-adapters.md)
- [確切托管確認狀態](../../reapplication/verification-status.md)

診斷候選套件唔替換生產發佈路線。
