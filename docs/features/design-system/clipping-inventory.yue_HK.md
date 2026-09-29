---
translation-of: clipping-inventory.md
source-sha256: 797543bde53e086ae250d2e5273d11996e6b03c8b452514339d77d7c8fc621c4
review-status: agent-drafted
---

> 英文原文：[Layout clipping inventory](clipping-inventory.md)

# 版面裁剪紀錄簿

Windows 桌面應用程式上發現嘅每一個版面裁剪缺陷，包括佢嘅元組、原因、修復同埋冇咗嘅證據。呢份檔案係手寫嘅，由 `ui-md3/tests/clipping-inventory.test.mjs` 機械檢查：每一行都需要一個 id、一個表面、佢被發現嘅元組、症狀、根本原因、一個呢個倉庫度存在嘅修復提交，同埋一個狀態。一行只可以話 `verified` 當佢嘅前後捕獲都存在於 `docs/screenshots/md3-everything/` 下面，而且係從實際構建嘅應用程式度拍嘅。

狀態：

| 狀態 | 意思 |
| --- | --- |
| `fixed-unverified` | 原始碼修復已提交；執行時捕獲對仲未存在 |
| `verified` | 前後捕獲存在，係從指定元組嘅構建應用程式度拍嘅 |
| `open` | 由探針或肉眼發現，仲未修復；呢一行標明咗啲阻礙 |

## 行

<!-- clipping-inventory:begin -->
| Id | 表面 | 元組 | 症狀 | 根本原因 | 修復提交 | 前 | 後 | 狀態 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| CJ-001 | 偏好對話框 | 150% 同 200%，任何語言，任何主題 | 對話框喺 100% 像素大小下打開並裁剪咗佢嘅下面行 | `SetSize(wxSize(780, 580))` 冇 `FromDIP` | 44ed39a18 | 待定 | 待定 | fixed-unverified |
| CJ-002 | 插件下載對話框（GUI_App 路徑） | 150% 同 200% | 對話框按 100% 大小設定咗，但 GUI_App 路徑度 MainFrame 路徑調整咗佢 | 冇調整大小嘅 `SetSize(270, 158)` 喺 `GUI_App.cpp` | 44ed39a18 | 待定 | 待定 | fixed-unverified |
| CJ-003 | 設備監察基礎面板 | 125% 及以上 | 打印機名稱標籤喺任意位置截斷；面板同標籤列固定喺 100% 寬度 | 三個省略號樣式 ORed 喺一個標籤上；`600x400` 同標籤列寬度冇調整 | 44ed39a18 | 待定 | 待定 | fixed-unverified |
| CJ-004 | 監察新增機器按鈕、PartSkip 標籤、對象表格頁面欄位、Tab 按鈕、StatusPanel 天數計數器、AMS 設定、建立預設、未儲存更改 | 150% 同 200% | 喺原始像素度大小嘅控制項裁剪咗佢哋自己嘅標籤喺高縮放度 | `SetMinSize` / `SetMaxSize` 度嘅字面 `wxSize(N, M)` | 44ed39a18 | 待定 | 待定 | fixed-unverified |
| CJ-005 | 每一個 wxBoxSizer 行（Print pill、Process title 前身） | 每一個元組（類級別） | 過度訂閱嘅行使到後面嘅項目零寬度；冇嘢溢出所以冇嘢可見 | wx 全額支付 proportion-0 項目同將後面項目交給剩餘部分 | 49a505a67 | prepare--en-light-comfortable--before.png | prepare--en-light-comfortable--after.png | verified |
| CJ-006 | 設備標籤佔位符頁面（「打印機連線」） | 每一個元組，1200x800 框架 | 標題喺頁面嘅頂部被截斷咗，冇得向上捲進去 | body 係一個弗萊克斯盒完全居中喺 100vh；一個較高嘅卡向上溢出到冇捲軸可以到達嘅地方 | fbfae7d38 | device--en-light-comfortable--before.png | device--en-light-comfortable--after.png | verified |
| CJ-007 | 準備操作欄、Slice / Print 選項片段 | 每一個元組 | 24 px 選項片段顯示成裸露綠色分割線，短咗兩像素嘅最小值，冇 chevron | 片段一旦光柵插字符被刪掉後就冇內容；SideButton 喺 sizer 分配下度量量咗佢 | bb4a0988a | prepare--en-light-comfortable--before.png | prepare--en-light-comfortable--after.png | verified |
| CJ-008 | 準備墨水行、顏色樣本按鈕 | 每一個元組（首個構建後捕獲） | 樣本加寬到 44 px 同畫咗第二個「1」喺徽章旁邊 | Plater 將樣本標籤設定為飾品 hack 嘅絲狀索引；工具包 Button 畫標籤 | bb4a0988a | evidence/ink-row-swatch-label--bbc9db5cf.png | prepare--en-light-comfortable--after.png | verified |
| CJ-009 | 主版標籤、視窗標題欄 | 每一個元組，1200 px 框架嘅首次顯示 | 標題欄（選單、項目芯片、視窗控制項）喺 1186 px 客戶端上保持 787 px 寬度直到標籤切換；視窗控制項坐喺視窗中間同其餘部分係空嘅 | 框架喺每一個大小事件度加寬咗欄，但寬度更新結束喺 update_responsive_title，佢呼叫 Realize，wxAuiToolBar::Realize 調整大小欄到佢嘅內容除非 wxAUI_TB_NO_AUTORESIZE 被設定 | 9b012c1e7 | home--en-light-comfortable--before.png | home--en-light-comfortable--after.png | verified |
| CJ-010 | 準備側欄、打印機卡 | 每一個元組（版面探針） | 一個庫存齒輪按鈕坐喺 0,0 顯示零寬度喺卡上，喺每一個 sizer 外面；用戶睇唔到，但一個活嘅庫存控制項標籤順序可以到達 | PlaterPresetComboBox 喺佢嘅父面板上構建一個舊版 ScalableButton；卡將佢替換為一個工具包編輯按鈕同從未隱藏過原始嘅（Process 卡已經做咗） | 96a054981 | home--en-light-comfortable--before.png | prepare--en-light-comfortable--after.png | verified |
| CJ-011 | 每一個工具包 SearchField（準備側欄、偏好、配置檔案、版本歷史、...） | 每一個元組 | 正則模式同構建器按鈕喺藥丸輪廓上面同下面度自己身上畫，一個空 44 px 位置坐喺尾邊 | 44 px 圖示按鈕喺 44 px 藥丸內部覆蓋佢嘅 1 px 輪廓；清除按鈕嘅位置即使被隱藏都永久預留 | baabd4e17 | prepare--en-light-comfortable--after.png | prepare-advanced--en-light-comfortable--after.png | verified |
| CJ-012 | 準備側欄開啟進階設定（每一行：打印機卡、墨水藥丸、搜尋藥丸、流程標籤欄） | 每一個元組（版面探針，1200 x 800） | 每一個側欄行喺 479 px 捲軸內度排列 1271 px 寬同喺側欄邊度裁剪；一個水平捲軸佔 17 px 高度；流程標籤欄結束喺「Otl」 | 重新家長 ParamsPanel 頭 sizer 將標題放喺 proportion 1（56 px 最小值）旁邊伸展空間 2、1 同 12；wxBoxSizer::CalcMin 按總比例（56 x 16 + fixed = 1271）調整最小值同 update_sidebar_scroll_body 尊重內容最小值作為虛擬寬度 | 3f4d8ffeb | prepare-advanced--en-light-comfortable--before.png | prepare-advanced--en-light-comfortable--after.png | verified |
| CJ-013 | 準備側欄、流程設定樹（類別藥丸欄同頁面區域） | 每一個元組（版面探針，1200 x 800，預設側欄寬度） | 類別欄隱藏速度/支撐/其他喺溢出後面（其他藥丸餓到 16 px）同設定樹喺 144 px 內部捲軸內度捲動，所以用戶必須拖動側欄更大先可以睇任何設定 | TabCtrl 嘅藥丸模式使用平欄隱藏唔合適嘅版面，ParamsPanel 係一個 proportion-3 項目（下限 240 px），擺喺捲動側欄本體內部，所以佢嘅頁面視圖只得到剩餘高度 | 92cd7bce7 | prepare-tree-categories--en-light-comfortable--before.png | prepare-tree-categories--en-light-comfortable--after.png | verified |
| CJ-014 | 長動作標籤嘅訊息對話框（噴嘴直徑選擇、自訂 `SetButtonLabel` 動作、"Do not execute"、"Go to ..."） | 每一個元組（類別級別） | 動作按鈕被壓到 44 DIP 下限，截短咗："Left..." 同 "Rig..." 而唔係 "Left nozzle: 0.4mm" 同 "Right nozzle: 0.6mm" | `MsgDialog::add_button` 畀每一個頁腳按鈕縮細（`SetAllowShrink(true)`），所以工具包 Button 報告嘅最細闊度係 44 DIP，而頁腳嘅彈性網格就啱啱淨係畀每個動作咁闊 | 9615c9418 | pending | pending | fixed-unverified |
| CJ-015 | 每一個工具包搜尋欄（Smart home、配置檔案及備份、版本歷史、準備側欄、偏好） | 每一個元組（類別級別） | 藥丸形外框嘅圓右端被覆蓋咗：佢嘅外框停止短咗同末端弧嘅一絲浮喺最後嘅圖示按鈕旁邊 | 尾部 40 px 圖示按鈕係一個子視窗，佢畫佢嘅整個正方形，喺 5 px 尾部填充度佢覆蓋咗 22 px 半徑末端嘅弧 | d27eadfdb | dialog-smart-home--en-light-comfortable--before.png | pending | fixed-unverified |
| CJ-016 | 鍵盤快速鍵、部分清單同快速鍵描述（雙語） | bilingual_en_yue_HK-light-comfortable | 緊湊標籤執行超過捲動面板："Objects list · 物件清"同描述喺對話框邊度被裁剪 | 雙語裝飾器只對包含排版器測量咗適合度，排版器喺捲動面板內度比可見面板寬 | d27eadfdb | dialog-keyboard-shortcuts--bilingual_en_yue_HK-light-comfortable--before.png | pending | fixed-unverified |
| CJ-017 | 配置檔案及備份、檔案清單 | 每一個元組（雙語時最差） | 清單顯示英文嘅一行同雙語模式下被截斷咗一半嘅行 | 資料檢視要求幾乎冇高度，所以清單只得到固定 720 x 700 對話框度文本上面剩下嘅 | 9670a437a | pending | pending | fixed-unverified |
| CJ-018 | 溫度校準、設定標籤（雙語） | bilingual_en_yue_HK-light-comfortable | "Start temp: · 開"、"End temp: · 結束"、"Temp step: · 溫度"：粵語喺標籤邊度被裁剪 | 標籤建立咗 120 px 寬，佢變咗佢哋嘅最小值，排版器計算咗排版器嘅鬆弛作為房間佢哋可以長到 | efaa98db2 | dialog-temperature--bilingual_en_yue_HK-light-comfortable--before.png | pending | fixed-unverified |
| CJ-019 | 回抽測試、步長欄位（每個帶單位嘅置中校準同預設欄位） | yue_HK-light-comfortable | 「0.1」被裁剪，「mm/mm」嘅第一個「mm」匿咗喺輸入框後面（最大體積流量速度測試入面「5 mm³/s」顯示成「5 /秒」）；英文模式下數字得 26 px，所以任何長過「0.1」嘅值都會被裁剪 | 置中欄位將單位畫喺最左邊、輸入框下面，而欄位建立時得 90 px 闊，冇替數字保留最少闊度（`a849963bf` 為數字保留位置；呢個提交將單位畫喺數字後面） | f3aab6af1 | dialog-retraction-test--yue_HK-light-comfortable--before.png | pending | fixed-unverified |
| CJ-020 | 每個訊息對話框嘅內文（雙語）：最新版本通知，同埋每一個普通訊息 | bilingual_en_yue_HK-light-comfortable | 「This is the newest versio」尾部得返一條空白，廣東話嗰行完全冇出現 | 內文喺一個捲動頁面入面，頁面嘅最小同最大尺寸都係按一行英文定死咗；雙語裝飾器之後先將標籤變成兩行，頁面大唔到，垂直捲軸就食咗句尾 | 2b8fa5d8b | dialog-check-for-update--bilingual_en_yue_HK-light-comfortable--before.png | pending | fixed-unverified |
<!-- clipping-inventory:end -->

CJ-013 喺嘗試 28（源 `92cd7bce7`）度被驗證：五個藥丸排列喺兩行（其他喺 y = 52），頁面視圖係 1672 px 高所以側欄本體（內容 2549 px 喺 645 px 客戶端）係唯一嘅捲軸，探針報告側欄冇餓到嘅行（轉儲 `probe/prepare-tree-categories--en-light-comfortable--attempt28.jsonl`）。

CJ-012 喺嘗試 23（`ba6ebba7baad232e` DLL 前綴，源 `2dcc26658`）度被驗證：側欄捲軸報告客戶端 479 x 645 同內容最佳 443 x 617 喺 1200 x 800，冇行比客戶端寬，冇水平捲軸；喺 1000 x 600 框架最小值處捲軸係 462 x 445 有一個垂直捲軸同設定樹保持佢自己嘅內部捲軸（捕獲 `prepare-advanced-minimum--en-light-comfortable--after.png` 同 `...-scrolled--after.png`，轉儲喺 `probe/prepare-advanced*-attempt23.jsonl` 下面）。

CJ-005 係類別，執行時版面探針為之存在。佢喺嘗試 13 矩陣上移到 `verified`（24 轉儲，12 元組，準備同偏好）：冇 `zero_sized` 同冇 `text_clipped` 尋找剩下，每一個 `starved`（48）、`clipped_by_parent`（12）同 `oversubscribed`（240）行係一個類別，主框架嘅 sizer 最小值（1000 x 951 px，由準備側欄嘅 900 px 最小高度驅動）喺 1200 x 800 度排列嘅視窗。呢啲行攜帶數字同冇可見效應喺呢個大小：側欄本體捲動、框架強制佢自己嘅 1000 x 600 最小值，冇控制項被畫短。提交著陸矩陣嘅讀者話冇 clipped-by-parent 視窗；佢發現呢啲十二個，全部喺果個一類，呢個註解係修正。

## 元組矩陣

矩陣係 4 個縮放 x 3 個語言 x 2 個主題 x 2 個密度 = 48 個元組每個表面。佢執行喺無頭驅動對抗 `install-dir/bambu-studio.exe`；轉儲係用 `node ui-md3/tests/layout-probe-report.mjs` 讀取。一次冇發生過嘅執行係記錄為冇執行，從未作為乾淨。

| 執行 | 構件提交 | 縮放 | 語言 | 主題 | 密度 | 發現 | 轉儲 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| baseline | not run | | | | | | |
| after | not run | | | | | | |

## 建議文章

- [Runtime layout probe](layout-probe.md)
- [Kit widgets added in the every-element sweep](kit-widgets-2026-09.md)
- [MD3 parity register](md3-parity-register.md)
