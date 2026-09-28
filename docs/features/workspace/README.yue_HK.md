---
translation-of: README.md
source-sha256: f0f37d788c8843f9a8e4183e1481480719262d6f26a9ce477f433a8905670582
review-status: agent-drafted
---

> 英文原文：[Workspace](README.md)

# 工作區

項目同工作流級功能嘅原住民應用程式：點項目係開啟、追蹤、版本化同點應用程式與用戶交流當工作進行時。

- [非封鎖通知](non-blocking-notifications.md)──資訊、
  警告同錯誤訊息表面作為角落吐司代替模態對話；
  決定對話保持模態。
- [通知中心](notification-center.md)──頂部欄嘅鈴聲
  開啟一個可搜尋、可過濾歷史每個吐司（500 項、儲存）、
  附帶多選、批量駁回、批量匯出四種格式同一個
  滑動確認批量刪除。
- [項目版本歷史](project-version-history.md)──本地、libgit2 支援
  每個項目嘅快照、可瀏覽/可復原從檔案 ▸ 版本歷史
  同頂部欄歷史晶片。
- [工作區包同規劃](workspace-bundles.md)──便攜分組
  項目、擁有編輯源、檢查表、規劃槽、提醒同
  日曆匯出。
- [類瀏覽器項目標籤](project-tabs.md)──一個標籤按項目附帶
  快照基礎切換、加新標籤同關閉優惠。
- [標籤設定同共用標籤條帶](tabbed-settings.md)──一個
  瀏覽器風格條帶後面項目標籤同設定部分：停靠
  邊緣（左預設對設定）、溢位選單、重新排序、固定、分組、
  四個標籤搜尋附帶正規表達式構建器、兩個批量關閉動作、按表面
  儲存同方向感知標籤表可存取性。
- [外部編輯器](external-editor.md)──可配置「開啟喺外部
  編輯器」對目前項目資料夾、附帶編輯器自動偵測。
- [配置設定 & 完全數據備份](config-profiles-backup.md)──匯出
  整個數據目錄（包括秘密、後面滑動確認門）、
  匯入佢喺另一部 PC 作為新設定、保持無限設定同得到
  每一個本地 Git 支援快照歷史。
- [匯出所有野、以所有格式](export-everything.md)──一個共用
  MD3 匯出對話對版本歷史、設定、設定、物件
  清單同打印統計：JSON/JSONL/YAML/TOML/XML/CSV/TSV/Markdown/HTML
  附帶無損/有損徽章同精確損失原因、UTF-8 加 LF/CRLF 標題、
  ZIP（miniz）或 7z（安裝 7-Zip、完整選項集包括 AES-256 同
  加密標題）。
- [設定自動歷史](preferences-history.md)──每個設定變更
  提交 BambuStudio.conf 到一個隔離本地 Git 倉庫（防抖、
  去重）、附帶一個瀏覽器同恢復旁邊活躍檔案語意。
- [設備風扇動作](fan-motion.md)──獨立遙測驅動部件同
  輔助風扇預覽附帶不同輸入同命令反饋。

## Postman 集合

不適用。呢啲係桌面工作區特徵附帶冇 HTTP 或 API
表面、所以冇 Postman 集合提供對呢個分類。
