---
translation-of: every-feature.md
source-sha256: 335e543441315d8ab625d4fdc193f8839cdd3af3be1697560e39d9a29e8bb714
review-status: agent-drafted
---

> 英文原文：[Every feature page](every-feature.md)

# 全部功能頁

**表面**：說明 > 全部功能、命令調色盤（`Ctrl+Shift+F`，打「Every feature」），或者開咗之後嘅本機工具分頁 → `FeatureFinderPanel`（`src/slic3r/GUI/FeatureFinder/FeatureFinderPanel.{hpp,cpp}`），背後係純數據目錄 `src/slic3r/GUI/FeatureFinder/FeatureCatalogue.{hpp,cpp}`。

設計：[design/local-tools-2026-11](../../../design/local-tools-2026-11/README.md)，FeatureFinder 呢塊板。

## 顯示乜嘢

所有必要功能擺埋一齊，分做語言同聲音、外觀、搵嘢同導航、紀錄、工具同安全。每一行有：

- 功能名同佢喺呢個版本住喺邊度，例如「檔案，本機檔案轉換器」或者「偏好設定，旁白」；
- 有嘅話就係真正用得嘅快捷鍵（`Ctrl+Shift+F`、`Ctrl+Shift+E`、`F1`、`Shift+?`）；
- 呢個版本修好咗嘅功能有 **已修好** 標籤；
- 一個 **開啟** 掣，做嘅嘢同選單項目或者設定完全一樣。

已經設計好但未放入呢個版本嘅功能會寫 **未起好**，冇開啟掣；呢個版本嘅每個功能都已經起好，所以而家冇功能會咁寫。呢部機開唔到嘅功能會講原因：例如學校模式開咗嗰陣，用字同搞笑程度設定會寫 **學校模式下隱藏**；如果佢嘅選單指令喺度未登記，就寫 **呢個系統用唔到**。

## 點樣搵功能

- 喺搜尋欄打字。普通文字會對功能名、位置、快捷鍵同額外關鍵字（搵「json」會搵到個人用字檔，「VHDX」搵到虛擬磁碟，「TOTP」搵到驗證器）。`.*` 掣會轉做正規表達式，用共用嘅正規表達式建立器。
- 晶片可以按組篩選，或者淨係顯示未起好嘅功能（有未起好嘅功能先會出現嗰個晶片）。
- 排序可以揀按組、A 至 Z（跟翻譯咗嘅名）或者最近用過。
- 最近開過嘅功能會喺清單上面出現，保留最後八個。
- 命令調色盤會將每個功能列做「Feature / 名」，連埋位置同原因，所以 `Ctrl+Shift+F` 一樣搵到。

## 開啟點運作

| 類型 | 開啟會做乜 |
| --- | --- |
| 一個設定 | 打開偏好設定去嗰一行，捲到佢、聚焦同閃一閃 |
| 一個選單指令 | 行同選單項目一樣嘅處理程序（用同一個 id 登記） |
| 命令調色盤 | 打開調色盤 |
| 一篇文章 | 打開離線文件文章 |

選單項目喺 `MainFrame.cpp` 用 `feature_command(id, handler)` 登記處理程序，所以顯示做用得嘅指令真係會行。設定用同命令調色盤跳轉一樣嘅行登記，所以搞笑程度滑桿、程式名同程式標誌嗰幾行而家都跳得到。

## 儲存嘅值

| AppConfig 鍵 | 值 |
| --- | --- |
| `feature_finder_sort` | `group`、`a-z` 或 `recent` |
| `feature_finder_recent` | 最多八個功能 id，最新嘅喺前面 |

`feature_finder_recent` 入面唔認識或者未起好嘅 id，讀嘅時候會被丟走。

## 保持老實

`tests/feature_catalogue.test.mjs` 會編譯 `tests/feature_catalogue_test.cpp`，仲會檢查源碼：

- 目錄入面每個 `command:` id 都喺 `MainFrame.cpp` 登記咗；
- 每個 `prefs:` 鍵都係登記咗嘅偏好設定行；
- 每篇 `docs:` 文章都存在，而且喺離線文件包入面；
- 每個圖示都喺附帶嘅 Material Symbols 字型入面；
- 未起好嘅功能冇動作，起好咗嘅有一個有效動作。

一個故意整壞最近開過次序嘅負面建置一定要失敗，證明測試真係捉得到問題。設計好嘅功能一出，就喺同一個提交將佢改做 `State::Ready`，加埋真正嘅動作。
