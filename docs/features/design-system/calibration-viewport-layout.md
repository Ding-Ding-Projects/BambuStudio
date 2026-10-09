# Calibration viewport layout

## Instruction pages

The calibration shell gives the visible page its available scroll-client width instead of centering a page with a 1100-DIP minimum. The existing page margins now follow the selected density. Instruction labels measure their original text at that width, keep their measured height, and refresh the outer scroll extent. The before/after image groups wrap as complete images.

Resize, show and DPI changes coalesce into a deferred reflow. A running reflow cannot schedule itself recursively. Changed scrollbar width schedules a subsequent measurement. Existing automatic label wrapping also responds to content changes. No printer command, step order, calibration value, translated string or save identity changes.

## Verification and limits

`node --test tests/calibration_layout.test.mjs` checks the actual adapters. Against commit `5568ae6ec28bb86cf481989da79d5582ca76dbfd`, all three cases fail; current source passes all three. `node tests/calibration_layout_fixture.test.mjs`, in an MSVC developer environment, builds only a non-window fixture outside the repository and exercises the production `CalibrationLayout.hpp`: five cases pass for measured widths at four scales, narrow/hidden bounds, height changes, deferred coalescing and state release. The existing preservation checks explicitly normalize only the new layout operations; actions, values and text remain compared with their original baselines.

The four compile-and-run fixtures in this article (`calibration_layout_fixture`, `calibration_minimum`, `calibration_reflow` and `calibration_result_viewport`) take their compiler from `tests/native_fixture_compiler.mjs`: MSVC `cl.exe` on Windows, run from a developer environment, and `$CXX` (default `c++`) at C++17 with `-Wall -Wextra` on any other host. The Linux contract gate therefore executes the same fixtures and their negative modes instead of stopping because `cl.exe` is missing.

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

## Intrinsic width correction

The shared page minimum is unspecified (`-1`) rather than explicitly zero. This removes the inherited 1100-DIP floor while preserving the calculated minimum of fixed-width descendants. Only the labels that actually wrap use an explicit zero minimum width. An indivisible descendant wider than the client area therefore contributes to the outer virtual extent and remains horizontally reachable.

`tests/calibration_minimum.test.mjs` compiles the actual page and advice initialization statements and their shared macro against a non-window model of wx effective-minimum and outer-scroll propagation. The fixture gives a fixed 600-DIP status control a 540-DIP page viewport, with real side padding, at 100%, 125%, 150%, and 200% scale. The previous source fails all four overflow cases and passes the bounded-label case (1/5); repaired source passes 5/5. This is a sizing-contract check, not native scrollbar or focus evidence. Result-table scroll ownership is still pending separate review.

## 原有內容闊度修正

共用頁面最小闊度改為未指定（`-1`），唔再明確設為零。咁樣可以移除 1100 DIP 限制，同時保留固定闊度子控制項計算出嚟的最小闊度。只有真正換行的標籤先用零最小闊度；比可用區域更闊的完整控制項會計入外層捲動範圍。

測試直接編譯正式頁面及提示卡的初始化語句與共用常數，模擬有效最小闊度傳到外層捲動區的過程。四種顯示比例下，600 DIP 狀態控制項放入 540 DIP 頁面再加邊距，舊版四項溢出檢查全部失敗，只有有界標籤一項通過；修正版五項全部通過。呢個只係尺寸契約檢查，唔係原生捲動條或焦點證據。結果表格捲動容器仍待獨立審核。

## Result tables

Automatic pressure-advance, multi-extruder pressure-advance and automatic flow-rate results now use a local `CalibrationResultViewport`, derived from the existing `MD3ScrolledWindow`. Each table keeps its original sizers, header order, direct child controls, values, extruder/tray identities, validation and actions. Complete tables scroll horizontally instead of wrapping columns independently. The three owning sizer items expand to their available width.

The viewport alone has a bounded minimum width. The surrounding page retains its unspecified minimum so other fixed descendants remain represented. Deferred size, show, DPI and result-content updates measure `GetSizer()->CalcMin()` directly, reserve the existing scrollbar thickness when horizontal overflow exists, update the local virtual size and refresh ancestors through the outer scroll host. Shorter result sets can reduce height because the previous viewport minimum is never the measurement source. Refresh requests coalesce; changed client width gets another deferred pass. No animation, product copy or calibration behavior was added.

Verification:

- `tests/calibration_results_layout.test.mjs`: baseline `eb07dfe7e6f61f5ae5601c42a179cecd0dbbbe46` has one pass and two failures; current source passes 3/3. The preservation case compares the complete source after only the exact viewport class, construction, expansion and refresh additions are normalized, and rejects an altered extruder identity.
- `tests/calibration_result_viewport.test.mjs`: compiles the actual production viewport class against a non-window geometry adapter. Six cases pass: local overflow propagation, narrow/wide/narrow transitions, shorter results, DPI change, hidden/zero-width recovery, and non-recursive ancestor refresh. Deliberately restoring an unbounded viewport width makes the overflow ownership case fail (5/6), then unchanged production source passes 6/6. The adapter models effective-minimum propagation rather than asserting only the assigned minimum value.
- The existing child-source preservation suite passes 5/5, giving 8/8 source cases with the new suite. Compiler output lives outside the repository.

No full application build or launch was performed. Native scrollbar rendering, wheel routing, focus reveal, keyboard operation and the full minimum-client-area/language/theme/scale matrix remain pending. The non-window adapter is not native wx rendering evidence.

## 結果表格

自動壓力提前、多噴嘴壓力提前及自動流量校準結果，改用沿用現有捲動控制項的本地水平捲動容器。原有表格排列、標題次序、直接子控制項、數值、噴嘴及材料槽識別、驗證同操作全部保留。整張表格水平捲動，唔會將互相關聯的欄位拆散換行。三個容器都擴展至可用闊度。

只有本地捲動容器限制最小闊度，外層頁面繼續保留未指定最小闊度。大小、顯示、DPI 及結果內容變動會合併成延後更新，直接量度表格排列器，需要時預留水平捲動條高度，再更新本地虛擬大小及外層捲動範圍。舊容器高度唔參與新量度，所以結果減少時可以縮短。冇新增動畫、產品文字或校準行為。

來源檢查修正前三項有兩項失敗，修正後三項通過；原有保留檢查五項通過。直接編譯正式容器類別的非視窗測試六項通過，刻意取消本地闊度限制後，溢出擁有權一項失敗，再用原來源重跑六項通過。今次冇完整編譯或啟動程式；原生捲動條、滾輪、焦點、鍵盤及全部畫面組合仍待實機驗證。

## Vertical wheel routing correction

The result viewport now handles vertical wheel events before its horizontal-only scroll helper can consume them. It finds the nearest outer `wxScrolledWindow`, copies the original event, preserves rotation, delta, axis and modifiers, translates the position through screen coordinates, changes the event object to the recipient, and dispatches through that owner's handler. The original event is consumed only when that dispatch reports handled. Horizontal events, missing outer owners and unhandled dispatches remain available to normal processing. `SetRevealOwner` is not used because it would disable local horizontal scrolling.

The non-window fixture executes the actual bound production handler. Six cases cover the copied payload and coordinates, horizontal-local behavior, nearest-owner selection, missing-owner fallback, unhandled dispatch and preservation of the local horizontal scroll rate. At `4e901cddc5d71949703cbd62adc575f8509b4580`, three of six fail. Removing the new binding deliberately produces the same three failures; restored source passes 6/6. Geometry remains 6/6 and related source preservation remains 8/8. This does not replace native wheel, focus or rendering verification.

## 垂直滾輪路由修正

結果容器會先處理垂直滾輪，避免只支援水平捲動的內層處理器吞咗事件。程式找出最近的外層捲動容器，複製原事件，保留轉動量、增量、軸向及修飾鍵，經螢幕座標換算位置，再交俾外層事件處理器。只有外層確認已處理，先消耗原事件。水平事件、冇外層容器及未處理情況繼續交回正常流程，亦唔會停用本地水平捲動。

測試執行正式綁定的處理程序，六項涵蓋事件內容與座標、水平留喺本地、最近容器、冇容器、未處理及水平捲動速度保留。舊版六項有三項失敗，刻意移除新綁定亦有相同三項失敗；還原後六項全部通過。幾何六項及來源保留八項繼續通過，原生滾輪、焦點同畫面仍待驗證。

## Default fixture coverage

Run `node tests/calibration_result_viewport.test.mjs` from an MSVC developer environment. It compiles once, then runs all six geometry cases and all six wheel-routing cases (12/12), returning failure if either mode fails. `--wheel` retains the targeted six-case route for baseline and negative runs.

Both default-path negatives were observed: `--negative-unbounded` gives geometry 5/6 plus wheel 6/6 and exit 1; `--negative-wheel` gives geometry 6/6 plus wheel 3/6 and exit 1. Restored production source gives 12/12 and exit 0. This corrects test discovery coverage only; production code is unchanged.

## 預設測試涵蓋範圍

喺 MSVC 開發環境執行 `node tests/calibration_result_viewport.test.mjs`，只編譯一次，再執行六項幾何及六項滾輪路由測試，合共 12/12。任何一組失敗都會傳回失敗狀態。`--wheel` 保留只執行六項滾輪檢查的用途。

預設路徑兩種刻意破壞都已驗證：取消闊度限制得到幾何 5/6、滾輪 6/6；移除滾輪綁定得到幾何 6/6、滾輪 3/6；兩者退出碼都係 1。正式來源 12/12，退出碼 0。今次只修正測試發現範圍，正式程式碼不變。
