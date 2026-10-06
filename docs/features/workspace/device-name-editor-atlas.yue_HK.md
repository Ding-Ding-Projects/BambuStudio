---
translation-of: device-name-editor-atlas.md
source-sha256: cc53c0a16d8f601bcdc864c8213a70579ccec6c149eba7f05348bc14c6730a3d
review-status: agent-drafted
---

> 英文原文：[Device-name editor presentation](device-name-editor-atlas.md)

# 裝置名稱編輯器外觀

實際可開啟嘅裝置名稱編輯器，依家用 Studio Atlas 表單間距同共用中型確認按鈕。
範圍只限 `SelectMachinePop.cpp` 同必要標頭嘅外觀，唔改命名規則或裝置指令。

## 入口同行為

真正路徑係 `MachineObjectPanel::on_mouse_left_up` 嘅編輯點擊範圍，經
`SelectMachinePopup::update_user_devices` 嘅 `EVT_EDIT_PRINT_NAME`，再呼叫
`EditDevNameDialog::set_machine_obj` 同 `ShowModal`。編輯值、驗證同最後
`DeviceManager::modify_device_name` 提交全部保留。

`on_edit_name` 繼續檢查非法字元、已修改預設後綴、空白名稱、開頭／結尾空格，同
超過 32 個字元嘅名稱。原有訊息、UTF-8 轉換、已選裝置識別同對話框結果不變。
排版或開啟唔會多發指令。輸入欄保留 `wxTE_PROCESS_ENTER`，Confirm 綁定不變，
冇新增按鍵或焦點處理。Escape／關閉仍由同一個對話框基底同標題控制。

## 版面

輸入欄填滿表單闊度，按實際字高量度，舒適／緊湊密度下限為 40/32 DIP，間距為
16/10 DIP。頁尾將原有 Confirm 放喺尾端，由共用按鈕量度翻譯標籤，唔再固定
72 乘 24 DIP。正常圓角同 DPI 生命週期保持啟用。

驗證訊息獨佔全闊一行，採用語義錯誤前景色。原有驗證處理會將真正訊息換行同要求
排版。新增嘅純外觀尺寸監聽器，喺驗證標籤增高時比較整個對話框排列器嘅量度下限
同可視區，必要時增加闊度或高度。訊息清空時唔會縮細正在使用嘅對話框，重入旗標
限制巢狀尺寸事件。保留原有置中呼叫，驗證內容增長唔會重新置中。

## 驗證同限制

`device-name-layout.test.mjs` 會編譯真正 `fit_validation_content` 方法，配合可
觀察尺寸／排列器替身。48 個斷言涵蓋兩種密度下限、100/125/150/200% 縮放、模擬
量度嘅長字／雙語頁尾同驗證內容、大字輸入欄、不作多餘縮放、回呼重入，同冇排列器
嘅情況。刻意刪除增長操作，就會令同一執行檔嘅內容尺寸斷言失敗。呢啲量度係測試
輸入，唔代表已經繪畫原生字體。

同 `6b4ebfa5a6207d66750a1a059124119229fa38cf` 比較後，六個函式內容完全相同：
驗證／改名提交、已選裝置設定、滑鼠編輯路徑、鍵盤路徑、主要啟動，同彈出清單嘅
使用者裝置事件綁定。

原生編譯、尺寸事件傳送、英文／廣東話／雙語、兩個主題、鍵盤焦點、Enter／Escape，
同真正裝置改名仍未驗證。冇啟動應用程式、完整建置、硬件指令或截圖。之後驗證要
用隔離測試裝置／名稱，唔可以露出帳戶資料或存取碼。

## 相鄰畫面邊界

最後發送確認仍喺 `ReleaseNote.cpp`；材料配對／補充喺 `AmsMappingPopup.cpp`；
噴嘴／材料重選喺 `DeviceTab/uiAMSBestPositionPopup.cpp`；局部縮時錄影選擇喺
`SelectMachine.cpp`；存取碼喺 `ConnectPrinter.cpp`；PIN／綁定／解除綁定喺
`BindDialog.cpp`；手動 IP 輸入喺 `ReleaseNote.cpp`。呢次冇重新設計呢啲畫面。
檢查過嘅來源有建立 `AmsMapingTipPopup` 同 `AmsTutorialPopup` 成員，但搵唔到真正
開啟呼叫，所以可達性仍然係清單缺口，唔會聲稱係已證實可開啟嘅流程。
