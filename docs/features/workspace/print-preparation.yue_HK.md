---
translation-of: print-preparation.md
source-sha256: f05c7ccead60cbcf8d2e0ab3bda997bda92406ec04e5a2dc892c14524c6afcba
review-status: agent-drafted
---

> 英文原文：[Print preparation and workflow navigation](print-preparation.md)

# 打印準備同工作流程導覽

原生工作區按視覺次序顯示「準備」、「預覽」、「打印」同「監察」。現有頁面編號唔會改動：「打印」係加喺最後嘅實際頁面，由導覽控制項安排顯示次序。主頁、項目、校準、墨水同多裝置保留原有入口；空間唔夠顯示嘅目的地，可以喺「更多工作區」搵返。

## 先檢視，唔會自動開工

打開「打印」唔會開始切片、提交工作、取消等候中嘅操作，亦唔會切換到「預覽」。用戶明確選擇「預覽」時，仍然會保留原有行為，包括可能開始切片。「打印」只讀取目前打印板、打印機預設、已有嘅切片估算同原有輸出可用狀態。打印機預設會清楚標示為預設，唔會冒充已連線嘅目的地。

頁面包括打印板準備狀態、目的地同對應設定說明、確認後繼續操作，以及打印摘要。摘要描述目前選取嘅打印板；所選輸出操作決定處理一塊定全部打印板。未有估算時會清楚說明。冇內容、未切片、切片中、準備就緒同暫時未能輸出，都有文字狀態。原有停用原因會顯示喺輸出按鈕旁邊，同時保留喺工具提示。

## 明確操作同確認步驟

「輸出選項」會喺啟動佢嘅控制項旁邊打開原有輸出模式選單。主要按鈕顯示所選操作嘅原有名稱。打印、傳送、匯出、多打印機同其他可用模式，都保留原有驗證同確認流程。目的地選擇同墨水對應仍然由嗰啲流程處理；呢個頁面唔會虛構已連線打印機、對應結果，或者保證一定可以打印。

「切片」、「切片並打印」同「切片並傳送」會呼叫「準備」原有嘅處理流程。啟動操作前會即時重新讀取可用狀態，避免上一塊打印板留低嘅啟用按鈕批准過時輸出。版本、打印板、墨水、等候後續操作同打印機檢查，仍然由原有流程決定。動畫唔會提交操作，亦唔會拖延回呼。「監察」保留原有網絡檢查、遙測、鏡頭同打印機控制。

## 外觀、語言同資料

卡片沿用共用 Material Design 色彩角色、字體、密度間距同 Device 強調色。可用闊度或者控制項實際最小尺寸需要時，兩欄檢視會改為上下排列。內容可以上下捲動，控制項可以換行，調整尺寸或者縮放後會重新量度標籤。

文字沿用 `_L` 翻譯路徑。英文、香港廣東話同雙語顯示仍然由程式嘅語言模式控制；現有語氣同本地字詞控制保持不變。「打印」原始碼已登記喺 `bbl/i18n/list.txt`，會納入正常字串擷取。新增廣東話翻譯標示為代理草稿，仍然需要熟悉廣東話嘅人覆核。

「打印」唔會新增儲存偏好、打印機憑證、指令歷史或者項目資料。讀取狀態唔會新增網絡請求。只有用戶明確操作先會進入原有流程，並保留該流程嘅存取同確認要求。

## 驗證同限制

`node --test ui-md3/tests/workflow-print-state.test.mjs` 會編譯正式狀態模型，檢查明確操作派送、忙碌同過時狀態、返回其他頁面，以及只讀取狀態時唔會執行操作。刻意移除正式可用狀態檢查後，同一組行為斷言會按預期失敗。

`python ui-md3/tests/test_workflow_print_localization.py` 會檢查擷取登記、GNU gettext 實際輸出、英文同廣東話字串目錄、編譯後翻譯查找、格式佔位符，以及配對文章。負面案例會移除登記或者必要翻譯，確認檢查可以發現遺漏。正常字串目錄驗證亦會檢查涵蓋率資料同確定性編譯。

以上檢查只證明原始碼同字串目錄行為。原生目標編譯、實際文字排版、無障礙互動、各種語言、縮放同主題組合，以及實際打印結果，仍然未經驗證。呢次功能改動唔會聲稱已有執行中畫面截圖或者實體打印證據。

## 字串目錄維護證據

合併工作流程同導覽原始碼後，擷取結果新增 143 個 POT 記錄，移除 26 個舊 POT 記錄。原有英文目錄未收錄嘅 114 個擷取訊息之中，呢次只加入 30 個工作流程同導覽訊息，其餘 84 個留待之後處理。原有英文同廣東話 PO 記錄全部保留，包括 185 個已經唔再擷取嘅英文記錄。POT 保持正式工具對目前原始碼嘅擷取結果，唔會手動混入舊鍵值。

已經用目前原始碼樹檢查 26 個被移除嘅 POT 鍵值。當中 23 個冇再出現相同文字；`Deleting…` 只喺內嵌 DeviceWeb 嘅語言 JSON 檔案出現，嗰啲檔案有獨立字串目錄路徑。另外兩句舊嘅不完整句子，仍然係已登記原生來源 `GUI_ObjectList.cpp` 同 `UserPresetsDialog.cpp` 入面嘅字串前段；GNU gettext 而家擷取完整拼接嘅 C++ 字串。呢兩項都唔係來源登記遺漏。以下列出嘅係被移除訊息識別文字，唔係新增介面文案。

<details><summary>26 個被移除嘅擷取識別文字</summary>

- `"Add an object to the build plate, select a material and printer that Helio supports, then slice."`
- `"Browse complete project snapshots saved automatically in a private local Git repository."`
- `"By default, Liveview will pause after 15 minutes of inactivity on the computer. Check this box to disable this feature during printing."`
- `"Click the Optimize/Enhance button to start your first optimization."`
- `"Could not load version history."`
- `"Deleting…"`
- `"Each object is removed from its plate together with all of its parts and instances. "`
- `"First Guide"`
- `"Great! Now click the Helio button to start optimization."`
- `"Keep liveview when printing."`
- `"LAN Connection Failed (Failed to start liveview)"`
- `"Liveview Retry"`
- `"Navigation rail"`
- `"Permanently delete %d notification entries from the history. This cannot be undone; the export button above keeps a copy first."`
- `"Permanently remove the selected entries from the history"`
- `"Released %s"`
- `"Selected commit: "`
- `"Slide to delete these entries permanently"`
- `"Start (UTC)"`
- `"Supported printers and materials"`
- `"The new profile folder could not be created."`
- `"The preset files are removed from the user preset folder and cannot be recovered "`
- `"This is your first time printing tpu filaments with the dual extruder machine.\nWould you like to watch a quick tutorial video?"`
- `"This is your first time slicing with the dual extruder machine.\nWould you like to watch a quick tutorial video?"`
- `"Version history is unavailable because its local repository could not be initialized."`
- `"Version history is unavailable for this project."`

</details>

嚴格嘅完整目錄來源成員檢查，仍然報告三個本來已經只存在於廣東話目錄嘅鍵值：`Interface motion`、`Reduce motion` 同 `Reduce motion settles supported transitions immediately. System follows your operating system preference.` 現有 CMake 目錄指令用 `--allow-unreferenced` 處理呢個已知擷取落差。沿用呢個選項再加 `--require-complete`，可以驗證 8,021 個已翻譯訊息。針對工作流程嘅測試會對全部 30 個新增鍵值嚴格檢查英文、POT 同廣東話成員資格，冇例外。

涵蓋率資料已經用現有作者工具更新，保留原有人工覆核資料；30 個新增翻譯全部標示為代理草稿。

缺少嘅擷取工具由 `scripts/i18n/update_catalogs.py` 取得：GNU gettext 套件 `gettext1.0-iconv1.19-shared-64.zip`，版本 `v1.0-v1.19`，來源係正式 `mlocati/gettext-iconv-windows` 發佈。已驗證 SHA-256 為 `c2f195fc4ed3df4070fb08ff88c2f724afd1eed4e28f859efae1000bb7ba1152`。擷取工具報告 11 個既有警告，第一個係 `src/slic3r/GUI/AMSMaterialsSetting.cpp:179` 嘅空白訊息識別碼。呢次工作流程修正唔會聲稱已修好呢啲警告或者留待處理嘅字串目錄缺口。
