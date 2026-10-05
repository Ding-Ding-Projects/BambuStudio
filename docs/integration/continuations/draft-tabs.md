# Preserved draft-tabs continuation

Historical source record from `codex/bambu-draft-tabs`. Its incomplete work and source-bound verification remain applicable until independently resolved.

# Continuation: Independent settings drafts

Status: unfinished preservation at the owner's requested closeout on 4 October 2026. No new implementation should start during this closeout.

Branch: `codex/bambu-draft-tabs`. Source checkpoint: `5010d13b6325abf5015e426252a2d13bad193553`. Base: `0c967a55786c07ef639a2cbefbe922b619c157d3`. The commit containing this document is the preservation tip; discover it with `git rev-parse HEAD`.

## Implemented source

Read the source diff and the dedicated feature documentation. This branch is not integrated into main and is not a verified release.

- `CLOSEOUT_PROMPT.md`
- `design/settings-drafts.md`
- `docs/features/settings-drafts.md`
- `src/slic3r/CMakeLists.txt`
- `src/slic3r/GUI/Plater.cpp`
- `src/slic3r/GUI/Preferences.cpp`
- `src/slic3r/GUI/SettingsDraftPanel.cpp`
- `src/slic3r/GUI/SettingsDraftPanel.hpp`
- `src/slic3r/GUI/SettingsDraftStore.cpp`
- `src/slic3r/GUI/SettingsDraftStore.hpp`
- `src/slic3r/GUI/SettingsDraftUndo.cpp`
- `src/slic3r/GUI/SettingsDraftUndo.hpp`
- `src/slic3r/Utils/UndoRedo.hpp`
- `tests/CMakeLists.txt`
- `tests/settings_drafts/CMakeLists.txt`
- `tests/settings_drafts/settings_drafts_tests.cpp`

## Verification

Source boundary and whitespace checks passed. Authored native tests were not compiled or executed.

## Required continuation

Undo must bind stable project-tab identity rather than filename and include attachments in memory accounting. Connect LocalConfigHistory recording/restoration. Fix duplicate host activation and removed-key handling. Verify close/restart, supported field controls, conflicts, no-activation Save as preset, localization, accessibility and layout.

Build the reconciled candidate through the supported Windows one-click route in an isolated, pinned build tree. Keep physical printer actions and host power changes out of scope. Preserve unrelated branches. Do not delete this branch or its checkout until completed work is verified, integrated and proved on remote main.

The preservation commit deliberately uses [skip ci] to avoid starting new release workflows during the requested closeout. No hosted result, compiled application, screenshot, installer, or release is claimed by that marker. Required verification remains outstanding.

## 廣東話交接

呢條分支只係保存未完成工作，唔代表已經編譯、驗證、合併或者發佈。上面列明已做嘅檢查同未完成項目。下一次先讀返差異，再修好接駁同執行原生驗證；唔好將保存當成完成。
