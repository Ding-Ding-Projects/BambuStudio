---
translation-of: README.md
source-sha256: 40a934dbbd272bc0b615a0d87661e231ea12d9c3eba39ac13b29f96c84a2210a
review-status: agent-drafted
---

> 英文原文：[Workspace](README.md)

# 工作區

項目同工作流級功能嘅原住民應用程式：點項目係開啟、追蹤、版本化同點應用程式與用戶交流當工作進行時。

- [可讀日期](readable-dates.md)：完整月份名稱、本地化日期、觀看者本地時間戳，同如實呈現嘅編譯版本資料。
- [批次操作](bulk-actions.md)：清單多選、Shift 範圍、Ctrl+A 本頁／Ctrl+Shift+A 全部符合項／Ctrl+I 反選；操作前檢視選取、會改動及略過數目，按模式改名附預覽及衝突偵測，破壞性批次操作經雙鍵確認。

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
- [可收埋嘅篩選同統計](collapsible-filters.md)──搜尋列、篩選列同統計面板可以收埋喺一個會記住狀態、用鍵盤操作到嘅標題下面；標題會讀出自己嘅狀態，收埋時一定會講明仲生效緊嘅篩選條件。
- [設備風扇動作](fan-motion.md)──獨立遙測驅動部件同
  輔助風扇預覽附帶不同輸入同命令反饋。
- [空白編輯器嘅「由預設開始」](blank-editor-presets.yue_HK.md)──本來一開就乜都冇嘅
  編輯器，會先提供預設；預設只會用出廠預設值（即係重設會還原嘅數值）、你已儲存嘅設定
  或者程式附帶嘅範本，並喺套用之前講明每個預設會建立同設定乜嘢。

- [打印準備同工作流程導覽](print-preparation.yue_HK.md)：只讀取狀態嘅打印檢視、原有明確輸出同確認流程，以及保留嘅工作區入口。原生畫面驗證仍然未完成。

## Studio Atlas 工作流程外觀

- [Prepare 檢查器同清單](../design-system/prepare-inspector-atlas.yue_HK.md)：原生準備控制及來源幾何量度。
- [打印設定外觀](print-setup-atlas.yue_HK.md)：打印機選取、檢視及原有傳送／匯出流程，包括重新開啟生命週期修正。
- [工作區組合](../design-system/workspace-atlas.yue_HK.md)：Overview、Files、Checklist、Notes、Calendar 排版，保留資料行為。

以上係來源實作記錄。凡文章列為未驗證嘅原生畫面、支援尺寸互動及完整語言／主題／比例矩陣，仍然未完成。

## Postman 集合

不適用。呢啲係桌面工作區特徵附帶冇 HTTP 或 API
表面、所以冇 Postman 集合提供對呢個分類。
