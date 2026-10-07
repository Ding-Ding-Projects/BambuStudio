---
translation-of: device-name-editor-atlas.md
source-sha256: 48cd805ac495930f8d2bf936e7e408da07fbe904c12ef99187dbf4c1603d6083
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
排版或開啟唔會多發指令。輸入欄保留 `wxTE_PROCESS_ENTER`，Confirm 嘅外觀包裝
只會呼叫原有驗證一次；驗證後對話框仍然顯示，先調整高度。冇新增按鍵或焦點處理。
Escape／關閉仍由同一個對話框基底同標題控制。

## 版面

輸入欄填滿表單闊度，按實際字高量度，舒適／緊湊密度下限為 40/32 DIP，間距為
16/10 DIP。頁尾將原有 Confirm 放喺尾端，由共用按鈕量度翻譯標籤，唔再固定
72 乘 24 DIP。正常圓角同 DPI 生命週期保持啟用。

驗證訊息獨佔全闊一行，採用語義錯誤前景色。標籤停用原生自動縮放，避免
`SetLabel` 喺原有驗證到達 `Wrap` 之前，同步按未換行闊度撐大對話框。
換行以編輯欄實際分配嘅闊度為準，對話框闊度唔會取自未換行驗證文字。
之前嘅尺寸事件監聽器已移除。

驗證完成後，純外觀調整先按分配闊度重新換行原文，再量度高度，預留真正標題、
輸入欄、頁尾同上下各 12 DIP 顯示邊距。顯示器工作範圍放唔晒時，垂直捲動區
仍然保留完整訊息。兩次有上限嘅排版會計埋捲軸闊度，重入旗標阻止巢狀調整。
可用高度唔大於零時，驗證可視區高度為零，唔會算術下溢或者硬塞最小高度。
固定輸入欄同頁尾仍然需要足夠顯示空間，唔會聲稱支援任意細嘅顯示範圍。
保留原有置中呼叫，驗證排版唔會重新置中。

## 驗證同限制

`device-name-layout.test.mjs` 會編譯真正 `Label::SetLabel`、`on_edit_name`、
確認包裝同 `fit_validation_content`。可觀察原生控制項替身模擬同步自動尺寸事件，
用模擬長翻譯訊息量度，實際執行 SetLabel、事件、Wrap 同調整嘅次序。
26 個斷言涵蓋 100/125/150/200% 縮放、闊度穩定、先換行後調整、工作範圍高度、
完整可捲動內容、巢狀調整保護、拒絕無效名稱，同有效名稱只提交一次，關閉後唔再調整。

同一測試套用 `5b77527856e266aa952392f88d031da05f67c715` 會喺
`SetLabel must not widen the dialog before Wrap` 失敗；修正後來源通過。
呢啲量度係測試輸入，唔係原生字體繪畫證據。測試用真正生產方法，唔係只提供
最後尺寸就當驗證過事件次序。

同 `5b77527856e266aa952392f88d031da05f67c715` 比較後，六個函式內容完全相同：
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
