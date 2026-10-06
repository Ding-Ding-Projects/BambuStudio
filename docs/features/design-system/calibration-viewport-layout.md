# Calibration viewport layout

## Instruction pages

The calibration shell gives the visible page its available scroll-client width instead of centering a page with a 1100-DIP minimum. The existing page margins now follow the selected density. Instruction labels measure their original text at that width, keep their measured height, and refresh the outer scroll extent. The before/after image groups wrap as complete images.

Resize, show and DPI changes coalesce into a deferred reflow. A running reflow cannot schedule itself recursively. Changed scrollbar width schedules a subsequent measurement. Existing automatic label wrapping also responds to content changes. No printer command, step order, calibration value, translated string or save identity changes.

## Verification and limits

`node --test tests/calibration_layout.test.mjs` checks the actual adapters. Against commit `5568ae6ec28bb86cf481989da79d5582ca76dbfd`, all three cases fail; current source passes all three. `node tests/calibration_layout_fixture.test.mjs`, in an MSVC developer environment, builds only a non-window fixture outside the repository and exercises the production `CalibrationLayout.hpp`: five cases pass for measured widths at four scales, narrow/hidden bounds, height changes, deferred coalescing and state release. The existing preservation checks explicitly normalize only the new layout operations; actions, values and text remain compared with their original baselines.

This is source and helper evidence, not native rendering evidence. The application was not built or launched for this change. Minimum client-area readability, keyboard scroll/focus, English/Cantonese/bilingual text, both themes and 100%, 125%, 150%, 200% display scales still require the native capture matrix. Preset rows and result tables are subsequent bounded changes.

## 校準說明頁版面

校準外框而家按捲動區可用闊度分配頁面，唔再硬性要求 1100 DIP。頁面邊距跟隨所選密度。說明文字按原文重新量度換行及高度，再更新外層捲動範圍；前後對照圖片以完整圖片為單位換行。

改變大小、顯示頁面及 DPI 變動會合併成延後重排。重排期間唔會遞迴排入自己；捲動條令可用闊度改變時，再量度一次。原有自動換行亦會處理文字更新。打印指令、步驟次序、校準數值、翻譯文字及儲存結果識別全部保留。

舊版本三項來源檢查全部失敗，現時三項全部通過；直接使用正式版面輔助程式的非視窗 C++ 測試有五項通過。呢啲只係來源及輔助程式證據，唔代表原生畫面已驗證。今次冇完整編譯或啟動程式；最低客戶區大小、鍵盤操作、三種語言模式、兩種主題及四種顯示比例仍然要由原生畫面驗證。預設材料列及結果表格另有獨立修改單元。
