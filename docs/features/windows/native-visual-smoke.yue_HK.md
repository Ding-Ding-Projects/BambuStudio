---
translation-of: native-visual-smoke.md
source-sha256: 4db3bd125f9bfcac1cbb00d65f8da98430f7d24ab88e2ecbb0cd0bc541358850
review-status: agent-drafted
---

> 英文原文：[Native Windows visual smoke test](native-visual-smoke.md)

# 本地 Windows 視覺煙霧測試

## 目的

`scripts/ci/Test-WindowsNativeVisual.ps1` 係一個確定性本地啟動同埋捕獲門禁用於未簽名嘅 Windows 候選。一份只係 Windows、環境門禁嘅 wxWidgets 表面係喺正常精靈、檔案或網絡啟動之前進入嘅。佢係意圖偵測一個喺展示確切測試視窗之前退出嘅流程、錯誤嘅場景 / 語言契約、缺失粵語文字、缺失或不可思議小嘅頂級視窗、重複場景同埋空白、低對比度或主題不一致嘅截圖。佢為呢啲場景產生 PNG 同埋 JSON 證據：

| 截圖 | 模式 | 證據主題 |
|---|---|---|
| `light-en.png` | `en` | 淺色 |
| `dark-yue_HK.png` | `yue_HK` | 深色 |
| `light-bilingual.png` | `bilingual_en_yue_HK` | 淺色 |

每次啟動都會收到一份喺 `RUNNER_TEMP` 下面新鮮嘅數據目錄同埋子流程環境中一份版本化場景請求。腳本同埋應用程式都要求 CI、GitHub Actions 同埋一份 GitHub 託管嘅執行器；一個未知協議 / 場景會被拒絕，而冇進入正常啟動。腳本會等待確切標題同埋 Win32 子文字契約，檢查所需或禁止嘅 CJK 存在，使用 `PrintWindow` 捕獲（退回到螢幕複製），同埋驗證維度、檔案大小、採樣顏色多樣性、對比度、淺色 / 深色亮度分離、唯一 SHA-256 雜湊同埋成對 RGB 差異。佢然後關閉或終止流程，只移除佢唯一嘅臨時數據。每場景元數據加上 `summary.json` 會記錄觀察到嘅文字同埋圖像量度。工作流會上載可用 PNG / JSON 檔案，即使一個稍後嘅斷言失敗，留下診斷證據。

## 執行邊界

腳本拒絕執行，除非 `-CiExecutionApproved` 被傳遞同埋 `CI=true`、`GITHUB_ACTIONS=true` 同埋 `RUNNER_ENVIRONMENT=github-hosted`。佢亦要求可執行文件係構建嘅 `bambu-studio.exe`，喺 `GITHUB_WORKSPACE` 下面，佢嘅姊妹資源目錄存在，同埋所有證據保持喺 `RUNNER_TEMP` 下面。呢個限制了未簽名二進制嘅自動執行到一個可拋棄 GitHub 託管 Windows 執行器。安裝程式行為矩陣使用同一個執行邊界。

喺準備呢項工作時，冇本地未簽名應用程式或安裝程式執行被執行。一份獨立本地 Windows 沙箱審查仍然要求明確行動時間確認，同埋應該使用沙箱本地數據目錄、冇用家憑據、喺可行時聯網被禁用。

## 煙霧測試唔證明咩

捕獲門禁唔係一份金色圖像像素差異套件、OCR 檢查、無障礙審計、字體族內省測試或平常產品螢幕遍歷。佢證明咗編譯 Windows wxWidgets 路徑可以轉譯版本化 MD3 證據契約；佢自己唔證明每份生產螢幕、Roboto / Microsoft JhengHei UI 選擇、CJK 字形、雙語揭示或淺色 / 深色令牌正確轉譯。呢啲聲明要求對上載證據嘅檢查同埋更寬階嘅手動或自動覆蓋。

## 驗證狀態

Windows 工作流係配置為喺本地構建同埋 C++ 測試之後執行三個場景，然後將證據發佈為 `BambuStudio_Windows_native_visual_<version>`。一次成功候選執行同埋對佢嘅截圖嘅人類審查仍然待機；冇未來執行或發行版本係喺呢度被代表為通過證據。
