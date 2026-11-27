---
translation-of: integration.md
source-sha256: c55d4cc9f08eb7d344777afa9f7170a5850020a5edd7fced3e828d492fb8f081
review-status: agent-drafted
---

# 應用程式整合清單

共用應用程式、偏好設定、指令面板、設定檔、翻譯目錄同建置檔案由整合工作負責。個人模式服務只修改自己負責嘅檔案。要將呢啲服務列為完整可用嘅介面功能，仲有以下工作。

- [ ] 平台所需工具齊備時，喺 `tests/CMakeLists.txt` 加入 `add_subdirectory(personal_modes)`。
- [ ] 加入 `PersonalModes/SchoolRuntime.hpp`，並喺 GUI 執行緒建立同管理一個 `PersonalModes::SchoolRuntime`；必須喺顯示翻譯或者個人文字之前建立。實作全部喺標頭檔內。
- [ ] 喺回呼內重建語言顯示、雙語登記表、設定同指令面板可用項目，更新 `PersonalVocabulary`，並依照保留嘅基本偏好重新顯示已開視窗。唔可以覆寫已儲存嘅語言或者趣味程度。
- [ ] 所有隱藏功能都經 `SchoolMode::available()` 判斷，包括直接用鍵盤或者程式啟用。改名後只可以用使用者揀選嘅顯示名稱。
- [ ] 將登記、驗證同代次管理接駁到 `LocalSecurity::Credentials` 同 `shared_mode_account`。唔可以用介面布林值代替憑據驗證。解釋刪除共用記錄會重設模式。
- [ ] 如實顯示原生監察、後備輪詢、記錄無法讀取同損毀等狀態，並保持模式控制元件可用。
- [ ] 接駁五個 `AttentionCallbacks`、儲存 `AttentionSettings`、登記本機歷史同匯出功能、傳送真實活動同每秒計時事件，並喺學校同兒童模式保留各項輔助。
- [ ] 加入旁白開關、語言選擇，以及英文同廣東話各自嘅語音、語速同音高。儲存穩定識別碼，並區分所選語音缺失、實際使用後備語音、無法使用同網絡能力不明。
- [ ] 透過 `TtsNarrator::set_quiet` 接收安靜、減少聲音同屏幕閱讀器運作狀態。拆除 GUI 前先呼叫 `TtsNarrator::shutdown`。
- [ ] 用 `TtsNarrator::external_mirror_status()` 標示已設定嘅 Home Assistant 係外部轉播，未能確認播放完成；唔可以將送出請求當作符合本機順序播放保證。
- [ ] 將餘下已本地化嘅 `say()` 呼叫改用 `say_tracks()`，令旁白語言獨立於介面。打印機事件已經用獨立語言軌。
- [ ] 將全部新控制元件、說明、狀態同披露文字本地化，加入設定同指令面板搜尋。每個新搜尋框旁邊都要有自己獨立嘅完整正規表達式編輯器。
- [ ] 透過已核准嘅設計流程製作控制元件，編譯真正嘅應用程式，並截圖驗證語言、暫停同恢復顯示、鍵盤操作、窄版面同高縮放。服務測試唔足以證明介面行為。

呢批服務工作冇新增可見介面，所以未有設計預覽、截圖或者完整設定流程。完成上述整合係交付條件。
