# Preserved readable-dates continuation

Historical source record; incomplete work remains pending combined acceptance.

# Continuation: Readable date formatting

Status: unfinished preservation at the owner's requested closeout on 4 October 2026. No new implementation should start during this closeout.

Branch: `codex/bambu-readable-dates`. Source checkpoint: `47f0c14452587654a8cce274d1e992e6fc9880ae`. Base: `0c967a55786c07ef639a2cbefbe922b619c157d3`. The commit containing this document is the preservation tip; discover it with `git rev-parse HEAD`.

## Implemented source

Read the source diff and the dedicated feature documentation. This branch is not integrated into main and is not a verified release.

- `docs/features/workspace/README.md`
- `docs/features/workspace/readable-dates.md`
- `resources/web/fila_manager/index.html`
- `resources/web/fila_manager/index.js`
- `resources/web/guide/23/23.js`
- `resources/web/guide/23/index.html`
- `resources/web/homepage3/home.html`
- `resources/web/homepage3/js/home.js`
- `resources/web/include/human-date.js`
- `src/slic3r/GUI/AboutDialog.cpp`
- `src/slic3r/GUI/ChangelogDialog.cpp`
- `src/slic3r/GUI/ConfigProfilesDialog.cpp`
- `src/slic3r/GUI/GUI_App.cpp`
- `src/slic3r/GUI/HumanDate.hpp`
- `src/slic3r/GUI/ImageGrid.cpp`
- `src/slic3r/GUI/MainFrame.cpp`
- `src/slic3r/GUI/MediaPlayCtrl.cpp`
- `src/slic3r/GUI/MultiTaskManagerPage.cpp`
- `src/slic3r/GUI/NotificationCenterPanel.cpp`
- `src/slic3r/GUI/ProjectHistoryDialog.cpp`
- `src/slic3r/GUI/WorkspacePanel.cpp`
- `tests/language_mode/CMakeLists.txt`
- `tests/language_mode/human_date_tests.cpp`
- `ui-md3/landing.html`
- `ui-md3/site/boot.js`
- `ui-md3/site/changelog.js`
- `ui-md3/site/core.js`
- `ui-md3/site/human-date.js`
- `ui-md3/site/views.js`
- `ui-md3/tests/human-date.test.mjs`
- `ui-md3/tests/site.test.mjs`

## Verification

JavaScript tests: 29/29 across human-date (3), site (24), changelog-date-row (2); seven syntax checks. Numeric-month mutation failed, intact version passed.

## Required continuation

Three authored native Catch cases remain unrun. Native compilation, timezone boundaries, visible date coverage and wider labels at all required language/theme/scale tuples remain unverified.

Build the reconciled candidate through the supported Windows one-click route in an isolated, pinned build tree. Keep physical printer actions and host power changes out of scope. Preserve unrelated branches. Do not delete this branch or its checkout until completed work is verified, integrated and proved on remote main.

The preservation commit deliberately uses [skip ci] to avoid starting new release workflows during the requested closeout. No hosted result, compiled application, screenshot, installer, or release is claimed by that marker. Required verification remains outstanding.

## 廣東話交接

呢條分支只係保存未完成工作，唔代表已經編譯、驗證、合併或者發佈。上面列明已做嘅檢查同未完成項目。下一次先讀返差異，再修好接駁同執行原生驗證；唔好將保存當成完成。
