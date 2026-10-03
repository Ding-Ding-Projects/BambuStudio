# Closeout handoff: open UI defects (2026-10-03)

Objective: fix the tracked open UI defects of this repository and land them on `main`: the plate type dropdown that still cut the two longest plate names at the default sidebar width (clipping inventory CJ-036, issue #51 item 3), the kit dialog caption that ignored a title set after adoption (ROADMAP follow-up), and the stale status counts in the README, roadmap, parity register and handoff.

State: three commits on `main`, written on a Linux host with no MSVC toolchain.

- `6994caf6f` Stack the plate settings label over its row-wide dropdown (`OptionsGroup::stack_full_width_label`, `OG_CustomCtrl` label band, `TabPrintPlate::build`; new `ui-md3/tests/plate-settings-stacked-rows.test.mjs`).
- `ad910deb2` Let the kit caption follow a dialog title set after adoption (`MD3DialogCaption::SyncTitle`, idle follow for captions adopted without a literal; `FeedDirectionDialog` adopts without the `Confirm` literal; new `ui-md3/tests/dialog-caption-title-sync.test.mjs`).
- The docs commit that carries this file: clipping inventory CJ-036 to `fixed-unverified`, feature articles in both languages, README counts, parity register header, ROADMAP lines, HANDOFF section.

Verification: `node --test ui-md3/tests/*.test.mjs` 427 of 432 on this host; the misses are the dim-sum online check (needs an authenticated `gh`) and the offline render (Chromium cannot open its DevTools port in this sandbox). Both new test files were seen failing on the previous source and go red again when one line of the fix is removed. Not compiled locally: the hosted build triggered by the push is the compile check. No capture exists yet for the stacked rows or the load dialog's title; CJ-036 stays `fixed-unverified` until a release is captured at 344 DIP in English, Cantonese and bilingual mode.

Evidence: issue #51 (start comment 5972780505, finish comment to follow with the build run), `docs/features/gcode-preview/preview-overlays.md`, `docs/features/design-system/native-controls.md`, `docs/features/design-system/clipping-inventory.md`.

Blockers: none in source. The hosted Windows build and a Windows capture host are needed for the compile verdict and the captures.

Next safe steps: read the hosted build verdict for the push; if red, fix and push again. When a release exists, capture Plate Settings at the default sidebar width in the three language modes and the load dialog on a two-extruder printer, post them on issue #51, and move CJ-036 to `verified`.

## 廣東話摘要

目標：修好呢個倉庫記錄在案嘅介面問題，推上 `main`：預設側邊欄闊度下仍然切走最長兩個打印板名嘅打印板類型下拉選單（裁剪清單 CJ-036，issue #51 第 3 項）、換咗標題之後唔跟住改嘅套件對話框標題列，同埋 README、roadmap、parity register 同 handoff 入面過時嘅數字。`main` 上有三個 commit：`6994caf6f`（打印板設定行上下排）、`ad910deb2`（標題列跟住標題）同帶住呢個檔案嘅文件 commit。本機 node 測試 432 個通過 427 個，其餘要 `gh` 或者 Chrome sandbox。呢部機冇 MSVC，推送之後嘅託管建置先係編譯檢查；亦未有擷圖，CJ-036 要等 release 擷圖先可以轉做 verified。
