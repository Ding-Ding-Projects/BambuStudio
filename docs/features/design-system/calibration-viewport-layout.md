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

## Preset rows and advice

The single-extruder selector now wraps each complete slot row, preserving its radio, checkbox, combo-box index and event bindings as one unit. Both containing panels expand so the wrapper receives actual available width. Multi-extruder row order is unchanged. The advice card expands, measures its client width minus its existing 20-DIP side padding, and refreshes the page and scroll host through the same non-recursive helper. Its printing-parameter groups wrap whole.

`tests/calibration_presets_layout.test.mjs`: previous source has one pass and two failures; current source passes all three. The identity test compares the complete slot constructor body with only the two exact layout substitutions normalized, and rejects an altered extruder index. The production width/state helper remains covered by the five-case C++ fixture. Native rendering remains unverified.

## 預設材料列及提示

單噴嘴選材區以完整材料列換行，每列的單選按鈕、核取方塊、選單索引及事件綁定一齊保留。兩層容器擴展至可用闊度；多噴嘴列次序不變。提示卡按自身闊度扣除原有左右各 20 DIP 邊距量度文字，再透過相同的非遞迴輔助程式更新頁面及捲動範圍。打印參數亦以完整組別換行。

舊來源有一項通過、兩項失敗，現時三項全部通過。索引檢查比較整段材料列建立程序，只正規化兩個指定版面改動，並確認改錯噴嘴索引會被發現。原生畫面仍未驗證。

## Height measurement correction

Both deferred adapters first remember and release the previous explicit label minimum height, then wrap, invalidate the cached best size, measure, and install the new height. This prevents the previous minimum from clamping `GetBestSize()` when a wider viewport or shorter content needs fewer lines. The previous height is used only to decide whether parent layout needs refreshing.

`tests/calibration_reflow.test.mjs` compiles the actual two production method bodies against a non-window label model that wraps character content and applies the wx-style best-size minimum clamp. Against `2ea14a5ea158e3311ad7680409243d51cd35b37a`, all four cases fail. The repaired methods pass all four: narrow-to-wide-to-narrow and shorter content for both instruction and advice labels. This proves the adapter ordering under that clamp, not native text metrics, rendering, focus or scrolling.

## 文字高度量度修正

兩個延後重排程序先記低並解除舊的最小高度，再換行、清除最佳大小快取、重新量度及設定新高度。咁樣視窗變闊或者文字縮短時，舊高度就唔會箍住新量度。舊高度只用嚟判斷需唔需要更新父容器版面。

非視窗測試直接編譯兩個正式程序，文字模型會按內容換行，亦會模擬最佳大小受最小高度限制的行為。修正前四項全部失敗，修正後四項全部通過，涵蓋兩種標籤由窄變闊再變窄，以及文字縮短。呢個結果只證明量度次序，唔代表原生字體量度、畫面、焦點或捲動已驗證。
