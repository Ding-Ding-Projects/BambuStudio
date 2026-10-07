---
translation-of: setup-index-atlas.md
source-sha256: 5a8cfe23bae57a2bf755adbc29702fcf244c3d0bfc9353aebb085dd38389a5fd
review-status: agent-drafted
---

> 英文原文：[Studio Atlas setup index](setup-index-atlas.md)

# Studio Atlas 設定索引

設定精靈索引用低層容器表面、圓角主要容器表示目前步驟、高層容器表示懸停，以及語意進度標記。原有步驟清單、縮排、目前／懸停狀態同點擊範圍推進方式仍然作準。文字按實際量度置中，原有延後計算最小寬度嘅流程包括末端留白。

項目標誌保留。標籤、導覽、選取語意、頁面次序、計時器同設定資料完全不變。

產品來源只改 `ConfigWizardIndex::on_paint`。基準係 `54810146717bbf4d531a17f4b6c471bd3300d190`；設計合約係 `3c8fe2708` 嘅 `design/workflow-refresh/surface-contracts.json`，頁面 ID 為 `wizard`。

執行 `node --test tests/config_wizard_atlas_index.test.mjs` 有四項來源檢查，包括拒絕刻意改動導覽路由嘅負向案例。檢查唔會編譯或執行原生繪畫。

本任務仍禁止即時設計流程同啟動應用程式。冇擷圖、執行版面量度、完整建置、鍵盤驗收、原生對比量度或 DPI／主題驗收。原有鍵盤同焦點行為係保留，唔係新實作。整合負責人須按支援組合驗證原生畫面；呢個繪畫單元應同設定行為及校準工作分開還原。
