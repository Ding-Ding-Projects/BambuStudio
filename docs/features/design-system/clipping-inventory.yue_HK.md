---
translation-of: clipping-inventory.md
source-sha256: 40036e3a4f4df03dd6d4355b07df0169ece377725348d3bc175b0451e90432b5
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
| CJ-014 | 長動作標籤嘅訊息對話框（噴嘴直徑選擇、自訂 `SetButtonLabel` 動作、"Do not execute"、"Go to ..."） | 每一個元組（類別級別） | 動作按鈕被壓到 44 DIP 下限，截短咗："Left..." 同 "Rig..." 而唔係 "Left nozzle: 0.4mm" 同 "Right nozzle: 0.6mm" | `MsgDialog::add_button` 畀每一個頁腳按鈕縮細（`SetAllowShrink(true)`），所以工具包 Button 報告嘅最細闊度係 44 DIP，而頁腳嘅彈性網格就啱啱淨係畀每個動作咁闊 | 9615c9418 | dialog-check-for-update--bilingual_en_yue_HK-light-comfortable--md3-v143.png | dialog-check-for-update--bilingual_en_yue_HK-light-comfortable--before.png | verified |
| CJ-015 | 每一個工具包搜尋欄（Smart home、配置檔案及備份、版本歷史、準備側欄、偏好） | 每一個元組（類別級別） | 藥丸形外框嘅圓右端被覆蓋咗：佢嘅外框停止短咗同末端弧嘅一絲浮喺最後嘅圖示按鈕旁邊 | 尾部 40 px 圖示按鈕係一個子視窗，佢畫佢嘅整個正方形，喺 5 px 尾部填充度佢覆蓋咗 22 px 半徑末端嘅弧 | d27eadfdb | dialog-smart-home--en-light-comfortable--before.png | dialog-smart-home--en-light-comfortable--md3-v150.png | verified |
| CJ-016 | 鍵盤快速鍵、部分清單同快速鍵描述（雙語） | bilingual_en_yue_HK-light-comfortable | 緊湊標籤執行超過捲動面板："Objects list · 物件清"同描述喺對話框邊度被裁剪 | 雙語裝飾器只對包含排版器測量咗適合度，然後測量咗可見寬度加對話框可能生長嘅房間（`d27eadfdb`，仲係喺 `md3-v150` 度被裁剪）；部分清單係一個固定寬度面板而描述坐喺一個捲動頁面入面，所以兩樣都唔同對話框一齊生長。呢個提交將一對話返英文一旦定居版面唔完全顯示佢 | 00b14ca67 | dialog-keyboard-shortcuts--bilingual_en_yue_HK-light-comfortable--before.png | dialog-keyboard-shortcuts--bilingual_en_yue_HK-light-comfortable--md3-v154.png | verified |
| CJ-017 | 配置檔案及備份、檔案清單 | 每一個元組（雙語時最差） | 清單顯示英文嘅一行同雙語模式下被截斷咗一半嘅行 | 資料檢視要求幾乎冇高度，所以清單只得到固定 720 x 700 對話框度文本上面剩下嘅 | 9670a437a | dialog-config-profiles-backup--bilingual_en_yue_HK-light-comfortable--md3-v143.png | dialog-config-profiles-backup--bilingual_en_yue_HK-light-comfortable--md3-v150.png | verified |
| CJ-018 | 溫度校準、設定標籤（雙語） | bilingual_en_yue_HK-light-comfortable | "Start temp: · 開"、"End temp: · 結束"、"Temp step: · 溫度"：粵語喺標籤邊度被裁剪 | 標籤建立咗 120 px 寬，佢變咗佢哋嘅最小值，排版器計算咗排版器嘅鬆弛作為房間佢哋可以長到 | efaa98db2 | dialog-temperature--bilingual_en_yue_HK-light-comfortable--before.png | dialog-temperature--bilingual_en_yue_HK-light-comfortable--md3-v150.png | verified |
| CJ-019 | 回抽測試、步長欄位（每個帶單位嘅置中校準同預設欄位） | yue_HK-light-comfortable | 「0.1」被裁剪，「mm/mm」嘅第一個「mm」匿咗喺輸入框後面（最大體積流量速度測試入面「5 mm³/s」顯示成「5 /秒」）；英文模式下數字得 26 px，所以任何長過「0.1」嘅值都會被裁剪 | 置中欄位將單位畫喺最左邊、輸入框下面，而欄位建立時得 90 px 闊，冇替數字保留最少闊度（`a849963bf` 為數字保留位置；呢個提交將單位畫喺數字後面） | f3aab6af1 | dialog-retraction-test--yue_HK-light-comfortable--before.png | dialog-retraction-test--yue_HK-light-comfortable--md3-v154.png | verified |
| CJ-020 | 每個訊息對話框嘅內文（雙語）：最新版本通知，同埋每一個普通訊息 | bilingual_en_yue_HK-light-comfortable | 「This is the newest versio」尾部得返一條空白，廣東話嗰行完全冇出現 | 內文喺一個捲動頁面入面，頁面嘅最小同最大尺寸都係按一行英文定死咗；雙語裝飾器之後先將標籤變成兩行，頁面大唔到，垂直捲軸就食咗句尾 | 2b8fa5d8b | dialog-check-for-update--bilingual_en_yue_HK-light-comfortable--before.png | dialog-check-for-update--bilingual_en_yue_HK-light-comfortable--md3-v153.png | verified |
| CJ-021 | 偏好設定，每一頁（雙語） | bilingual_en_yue_HK-light-comfortable | 頁面多咗條橫向捲動列，每行尾嘅控制項（語言清單、搞笑程度滑桿、開關、登入區域）被推出視線以外；「No warnings when loading 3MF with modified G-codes · ...」呢類行標題變成一行 629 px，壓住自己個開關，仲超出 533 px 闊嘅頁面 | 偏好設定頁唔會跟住對話框變闊，而係捲動；但雙語裝飾器畀標籤當對話框會加闊（最多 40 %），又將偏好設定按 320 DIP 換行嘅標籤配對成冇換行嘅一行；喺一行入面會拉闊嘅標籤亦將嗰行剩低嘅位計咗兩次（2e80091ef 修正）。md3-v155 照樣打橫捲：搞笑程度同表情符號嗰幾行從來冇經過裝飾器，因為偏好設定自己砌好「English · 廣東話」一行，大約 430 px 塞入 356 px 一欄 | dd95aace8 | preferences-general--bilingual_en_yue_HK-light-comfortable--md3-v151.png | preferences-general--bilingual_en_yue_HK-light-comfortable--md3-v160.png | verified |
| CJ-022 | 冇單位嘅設定欄位，同埋程式設成空白字串嘅文字（粵語） | yue_HK-light-comfortable | 準備側邊欄打印設定入面兩個數值輸入框嘅數字格闊 0 px，「10」同「1」完全睇唔到，文字欄亦由 105 px 縮到 45 px；單位位置顯示「Project-Id-Version: Bambu Studio ...」嘅開頭；偏好設定 > 使用者入面，「自動填充先前登入嘅賬號。」嘅說明係成個英文目錄標頭 | gettext 目錄會用標頭回答空白訊息；wx 自己查嗰陣會拒絕空白字串，但粵語模式搵唔到翻譯時會直接查英文目錄，嗰度冇攔住，所以每個欄位嘅空白單位（`_L(m_opt.sidetext)`）同原始碼入面每個 `_L("")` 都變成咗標頭 | c591f1b39 | preferences-search-autofill--yue_HK-light-comfortable--md3-v151.png | preferences-search-autofill--yue_HK-light-comfortable--md3-v157.png | verified |
| CJ-023 | 準備側邊欄，完整打印設定樹 | 每個組合（1200 x 800 視窗） | 每個數值欄都超出側邊欄右邊；廣東話分類按鈕按一個睇唔到嘅闊度換行，「支撐」有一半喺外面 | 分區條（墨水／打印設定／物件）以 128 DIP 闊放喺內容左邊，同內容共用同一個窗格，但每個窗格闊度都只計內容：設定樹喺 480 px 嘅窗格只分到 334 px，但佢要大約 417 px | 11cf45423 | prepare--en-light-comfortable--md3-v151.png | prepare--en-light-comfortable--md3-v157.png | verified |
| CJ-024 | 準備側邊欄，打印設定標題（粵語） | yue_HK-light-comfortable | 顯示「打印設…」而唔係「打印設定」 | 自從 CJ-012 修正之後，標題係固定項目，但仍然保留 56 DIP 嘅最細闊度，所以永遠唔會加闊到佢 60 px 嘅文字 | 11cf45423 | prepare--yue_HK-light-comfortable--md3-v151.png | prepare--yue_HK-light-comfortable--md3-v157.png | verified |
| CJ-025 | 雙語模式配對咗嘅段落標題（溫度校準「SETTINGS」） | bilingual_en_yue_HK-light-comfortable | 「SETTINGS · 設」：廣東話喺標題自己嘅邊緣被切 | 段落標題用 GDI+ 逐個字畫大階、加字距，但量度最佳闊度嗰陣用普通 GDI 同成串字嘅闊度，所以要求嘅位比畫出嚟嘅少 | 32a36b134 | dialog-temperature--bilingual_en_yue_HK-light-comfortable--md3-v154.png | dialog-temperature--bilingual_en_yue_HK-light-comfortable--md3-v158.png | verified |
| CJ-026 | 偏好設定，打開對話框之後先顯示嘅每一頁：使用者、3D、其他（雙語） | bilingual_en_yue_HK-light-comfortable | 每段會換行嘅描述排成英文喺上粵語喺下，但粵語嗰行畫咗喺下一行標題底下，淨係見到啲字嘅頂 | 裝飾器改完標籤之後會重新排對話框，但對話框重排嗰陣頁面大細唔變，所以頁面自己嘅 sizer 從來冇行過，變高咗一行嘅標籤下面啲行原封不動 | a07353987 | preferences-3d--bilingual_en_yue_HK-light-comfortable--md3-v155.png | preferences-3d--bilingual_en_yue_HK-light-comfortable--md3-v161.png | verified |
| CJ-027 | 有乜新嘢，「從」同「至」日期欄 | 每個組合（日先嘅地區設定） | 提示字「YYYY-MM-DD / DD/MM/YYYY」喺英文被切成「YYYY-MM-DD / D」、粵語「YYYY-MM-DD / [」、雙語「YYYY-MM-DD / I」 | 日期欄固定 132 DIP，窄過佢自己嘅提示字；版面探針唔量度提示字，所以冇任何掃描報告過 | 1af648025 | dialog-what-s-new-changelog--en-light-comfortable--md3-v158.png | pending | fixed-unverified |
<!-- clipping-inventory:end -->

CJ-014、CJ-015、CJ-017 同 CJ-018 已經用冇加過任何嘢嘅發佈套件，喺隱藏桌面上驗證咗（2026-09-29）：雙語模式下，最新版本訊息喺 `md3-v148` 完整畫出「OK · 確定」，而 `md3-v143` 就畫成「OK ·...」（CJ-014）；喺 `md3-v150`，搜尋欄完整畫出佢嘅圓形右端（CJ-015），設定檔清單完整顯示嗰一行（CJ-017），溫度校準嘅標籤維持英文，廣東話放喺提示框，唔再被裁走（CJ-018）。CJ-016 喺 `md3-v150` 仍然被裁；佢嘅修正係 `00b14ca67`。

CJ-020 已經用 `md3-v153`（目標 `1757d880f`，套件 SHA-1 `3a97afd5d4e083e92a40c72d2c62310e66dcd8c2`，所有匯入都搵到）驗證：雙語模式嘅最新版本訊息完整顯示「This is the newest version.」，下面有「已經係最新版本。」，冇捲動列。CJ-016 同 CJ-019 已經用 `md3-v154`（目標 `00b14ca67`，套件 SHA-1 `8ac42b12ec13fd2e814690414882e9926cdd0a3f`，所有匯入都搵到）驗證：雙語模式下，鍵盤快速鍵嘅每個標籤同描述都喺對話框入面（「Objects list」完整維持英文，粵語放喺工具提示），回抽測試嘅步長顯示「0.1 mm/mm」，單位喺數字後面，粵語同英文都係。

CJ-021 第一個修正 `2e80091ef` 冇修好發佈版：`md3-v155`（目標 `c7309b889`）嘅雙語偏好設定 > 一般照樣打橫捲，每行 681 px 闊，頁面得 560 px。裝飾器嘅 fit 規則冇錯，但從來冇喺搞笑程度同表情符號嗰幾行行過，因為佢哋嘅 helper 自己砌好「English · 廣東話」一行；`dd95aace8` 改為將呢啲配對交畀雙語登記表。同一個 commit 亦修正咗版面探針：佢將每個 sizer 邊框計咗兩次（`wxSizerItem::CalcMin()` 本身已經計埋），所以報雙語偏好設定底部嗰行按鈕「需要 799 px、有 783 px」，其實三粒按鈕啱啱好填滿。`dd95aace8` 之前量到嘅所有 `oversubscribed` 數字，包括下面 CJ-005 註解入面嗰 240，都多咗同樣嘅數。`md3-v155` 其中一張嗰頁嘅擷圖冇咗「Reset all warning dialogs」掣，但緊接住嘅轉儲將佢放喺 x = 12；同一輪嘅 3D 同外觀擷圖都有佢，所以嗰張擷圖係影正重繪嘅時候。

CJ-022、CJ-023 同 CJ-024 已經用 `md3-v157`（目標 `11cf45423`，套件 SHA-1 `1d14417aa7037d47d7b7e5c1b2de75172dde4dab`，所有匯入都搵到）驗證：粵語模式喺偏好設定搜尋 自動填充，嗰行自動填充冇描述，而 `md3-v151` 係成個翻譯檔檔頭（CJ-022）；準備側欄英文同粵語都將每個打印設定數值欄連單位完整顯示，標題、搜尋同預設組合嘅圖示全部喺側欄入面（CJ-023）；粵語打印設定標題完整顯示「打印設定」（CJ-024）。同一個發佈版嘅雙語偏好設定 > 其他搜尋欄有提示字，所以 `md3-v155` 一張擷圖冇提示字嘅情況冇再出現。

CJ-025 已經用 `md3-v158`（目標 `32a36b134`，套件 SHA-1 `8613eadca952103e32a2cb2bfbd348b02b18113e`，所有匯入都搵到）驗證：溫度校準喺雙語模式完整顯示「SETTINGS · 設定」，所有單位都係「°C」，粵語模式亦係「°C」；最大流量顯示「mm³/s」，有乜新嘢嘅日期用「 · 」分隔。同一次掃描搵到 CJ-027：有乜新嘢嘅日期欄喺三種模式都將提示字「YYYY-MM-DD / DD/MM/YYYY」切走。版面探針唔量度提示字，所以之前冇任何掃描報告過；`1af648025` 令每個欄有提示字咁闊，日期行亦可以換行。

CJ-021 已經用 `md3-v160`（目標 `dd95aace8`，套件 SHA-1 `57a99f72f9cb99efd1ff7e299be5c3b581838678`，所有匯入都搵到）驗證：雙語偏好設定 > 一般冇打橫捲動列，最闊嗰行喺 556 px 完結，頁面有 560 px，語言清單、兩條搞笑程度滑桿、表情符號開關同地區、單位清單都喺頁面入面。每條滑桿下面嘅來源行仍然係英文，粵語喺工具提示；`4d5bfc93f` 令佢換行，配對就可以排成兩行。

CJ-026 已經用 `md3-v161`（目標 `a0e408559`，套件 `2.8.4608`，SHA-1 `a2c80a7ef125f3167baa06e891f124832e2f8f58`，所有匯入都搵到）驗證：雙語 3D 同其他頁嘅每段兩行描述都完整顯示粵語嗰行，下一行喺佢下面先開始。同一個重排亦令配對咗嘅「What does this change? · 呢個會改變啲乜？」掣保留配對：之前個掣喺頁面入面保持舊闊度，重新檢查以為佢被擠窄，就送返英文。嗰輪有兩張擷圖影正重繪（工具欄風格清單、一行粵語），另一張擷圖都完整顯示。

CJ-026 喺同一輪 `md3-v155` 搵到：偏好設定打開之後先顯示嘅每一頁（使用者、3D、其他），每段排成兩行嘅描述嘅粵語嗰行都畫咗喺下一行標題底下。`a07353987` 會重排裝飾器改過嘅每個標籤周圍嘅頁面。嗰輪嘅雙語「其他」擷圖仲見到搜尋欄冇咗提示字，但一般、使用者、3D 同外觀嘅擷圖都顯示「Search settings · 搜尋設定」；只見過一次，下一個發佈再檢查，先決定要唔要開一行。

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
