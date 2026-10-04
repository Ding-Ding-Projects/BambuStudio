# Continuation: Menu lifetimes and printer switching

Status: unfinished preservation at the owner's requested closeout on 4 October 2026. No new implementation should start during this closeout.

Branch: `codex/bambu-native-lifecycle`. Source checkpoint: `4e4d192b9cdb0ca222b8c9da768fa24586f1822f`. Base: `0c967a55786c07ef639a2cbefbe922b619c157d3`. The commit containing this document is the preservation tip; discover it with `git rev-parse HEAD`.

## Implemented source

Read the source diff and the dedicated feature documentation. This branch is not integrated into main and is not a verified release.

- `CLOSEOUT_PROMPT.md`
- `docs/features/prepare/native-lifecycle.md`
- `src/slic3r/GUI/Plater.cpp`
- `src/slic3r/GUI/PresetComboBoxes.cpp`
- `src/slic3r/GUI/Widgets/ComboBox.cpp`
- `src/slic3r/GUI/Widgets/ComboBox.hpp`
- `src/slic3r/GUI/Widgets/DropDown.cpp`
- `src/slic3r/GUI/Widgets/DropDown.hpp`
- `src/slic3r/GUI/Widgets/MD3Menu.cpp`
- `src/slic3r/GUI/Widgets/MD3Menu.hpp`
- `tests/sidebar_filament_combos/CMakeLists.txt`
- `tests/sidebar_filament_combos/dropdown_lifecycle_tests.cpp`
- `ui-md3/tests/native-lifecycle.test.mjs`

## Verification

Production-body lifetime doubles: 16 assertions in 8 cases; source contracts: 7/7; stale-generation mutation failed as expected, restored version passed.

## Required continuation

Full native build, wx popup dispatch, both Ink deletion paths, canceled/missing presets, P1S/H2C and multi-plate runtime reproduction remain unverified.

Build the reconciled candidate through the supported Windows one-click route in an isolated, pinned build tree. Keep physical printer actions and host power changes out of scope. Preserve unrelated branches. Do not delete this branch or its checkout until completed work is verified, integrated and proved on remote main.

The preservation commit deliberately uses [skip ci] to avoid starting new release workflows during the requested closeout. No hosted result, compiled application, screenshot, installer, or release is claimed by that marker. Required verification remains outstanding.

## 廣東話交接

呢條分支只係保存未完成工作，唔代表已經編譯、驗證、合併或者發佈。上面列明已做嘅檢查同未完成項目。下一次先讀返差異，再修好接駁同執行原生驗證；唔好將保存當成完成。
