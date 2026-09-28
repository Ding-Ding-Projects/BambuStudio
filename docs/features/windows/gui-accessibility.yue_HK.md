---
translation-of: gui-accessibility.md
source-sha256: d2ead29b074068a1d06e633ac099e5aa12c03d69fd1c5bf5aaf245448162046d
review-status: agent-drafted
---

> 英文原文：[Keyboard, assistive, and responsive GUI accessibility](gui-accessibility.md)

# 鍵盤、輔助同埋回應式圖形用戶介面無障礙

## 行為

本地 Windows 應用同埋佢嘅分組網頁表面共用同一互動基線：執行一個動作嘅控件無需指標可到達、暴露一個程序名稱同埋狀態、同埋顯示一個可見焦點指示器。無障礙波涵蓋三層：

- **本地 wxWidgets 控件：** Slice 同埋Print 拆分動作、佢嘅選項選單、分割選擇器、文字連結、墨水分配器設定、標籤、媒體動作同埋打印機控件接受鍵盤焦點同埋保留佢嘅現存 wx 事件契約。彈出選單支援方向導航、Home 同埋 End、Escape 同埋焦點恢復。自訂控件暴露 `wxAccessible` 角色、名稱、狀態同埋預設動作而不是呈現為匿名繪製視窗。
- **墨水分配器 DeviceWeb 頁面：** 標籤、篩選、對話框、可排序表標題、可展開分組、選擇控件、分頁同埋通知使用語義瀏覽器控件。對話框包含焦點、用 Escape 關閉同埋返回焦點到打開者。工具欄同埋翻譯標籤喺狹窄寬度同埋瀏覽器縮放時回流而唔係剪裁。
- **項目同埋設定指南資源：** 自有動作係按鈕或連結、瀏覽器縮放保持可用、動態生成遺留動作接受鍵盤相容語義、同埋焦點保持可見。項目佈局使用有界流動寬度而唔係固定桌面專用畫布。

資訊同埋成功網頁通知喺逾時後關閉。警告同埋錯誤保持直到被關閉。本地只感謝消息使用現存角落通知漏斗；提示需要一個決定保持模式。

## 設定

冇單獨無障礙開關。鍵盤操作、無障礙名稱、回應式佈局同埋警告同埋錯誤持續係無條件嘅。

現存應用語言同埋外觀設定仍然控制翻譯副本、淺色或深色顏色、密度同埋字體。DeviceWeb 使用英文作為資源後備。英文、香港粵語（`yue_HK`）同埋雙語呈現使用分組主要同埋次要標籤喺遷移墨水分配器表面上、所以兩個語言唔依賴換行串連固定高度文字。

運動跟隨操作系統偏好、喺表面動畫嗎。減少運動下、自訂本地開關同埋攝像頭過渡同埋自有網頁過渡捲到佢們穩定狀態。

## 鍵盤摘要

- `Tab` 同埋 `Shift+Tab` 喺邏輯順序中通過互動控件移動。
- `Space` 或 `Enter` 激活按鈕、連結、拆分按鈕分段同埋其他類按鈕自訂控件。分割選擇器喺對應按鍵放開時提交一次、所以鍵盤自動重覆唔振盪選擇或發出重複命令。
- 方向鍵喺選項選單、標籤、分割選擇同埋值控件中移動、當平臺慣例期望佢們時。
- `Home` 同埋 `End` 選擇第一個或最後啟用彈出選項。
- `Escape` 關閉一個彈出或對話框同埋恢復焦點到呼叫控件。

## 失敗模式同埋限制

- 一個禁用或隱藏控件對鍵盤同埋無障礙激活保持不可用。
- 如果一個本地化標籤必須視覺縮短、佢嘅完整無障礙名稱同埋工具提示保持可用；佈局使用測量最小尺寸、然後才求助於省略。
- WebView2 內容喺本地 `PrintWindow` 捕獲路徑上唔出現喺呢個構建主機上。分組 DeviceWeb、項目同埋指南頁面因此用佢們嘅瀏覽器構建同埋資源測試驗證；本地幀同埋控件通過資料庫嘅離線 Windows 工具驗證。
- 資料庫範圍 DeviceWeb lint 目標包含無關先前存在失敗外帶變化墨水分配器檔。變化 DeviceWeb 檔被單獨檢查直到那個現存債務被解決。

## 安全考慮

無障礙元資料只包含相同用戶可見標籤同埋狀態已喺螢幕上呈現。無模式、項目、憑證、打印機存取碼或通知有效載荷被發送到新服務。

對話框只喺打開時保留焦點同埋關閉時移除佢們嘅文件監聽器。自有網頁通知渲染消息文字作為文字、而唔係可執行標記。移除縮放抑制唔授予頁面內容任何其他本地橋接能力。

## 驗證

源同埋資源契約被設計用以失敗喺修復前行為上。維護本地檢查係：

```bash
cmake -DBAMBU_SOURCE_DIR=. -P tests/native_shared_controls/native_shared_controls_accessibility_contract.cmake
```

```bash
node --test tests/web_resources_accessibility.test.mjs ui-md3/tests/md3-conversion-contracts.test.mjs
```

```bash
npm --prefix src/slic3r/GUI/DeviceWeb/device_page run test:i18n
```

```bash
npm --prefix src/slic3r/GUI/DeviceWeb/device_page run test:a11y
```

```bash
npm --prefix src/slic3r/GUI/DeviceWeb/device_page run build
```

喺 2026-07-30、交付候選通過晒所有三個聚焦本地無障礙契約、所有 10 結合自有網頁同埋 MD3 契約、兩個 DeviceWeb 聚焦測試、變化檔 ESLint、TypeScript 編譯同埋 Vite 生產構建。MSVC 編譯修復 GUI 庫同埋連結完整發佈應用。確切發佈 DLL 同埋實際應用無頭證據被記錄喺 `HANDOFF.md` 後最後重建同埋捕獲；源專用成功唔被代表作執行時證明。
