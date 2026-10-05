# Preserved local-history continuation

Historical source record; incomplete work remains pending combined acceptance.

# Continuation: Unified local history and printer incidents

Status: unfinished preservation at the owner's requested closeout on 4 October 2026. No new implementation should start during this closeout.

Branch: `codex/bambu-local-history`. Source checkpoint: `f58ed09c69931fbc28975ccc572f11165483ef1c`. Base: `0c967a55786c07ef639a2cbefbe922b619c157d3`. The commit containing this document is the preservation tip; discover it with `git rev-parse HEAD`.

## Implemented source

Read the source diff and the dedicated feature documentation. This branch is not integrated into main and is not a verified release.

- `CLOSEOUT_PROMPT.md`
- `src/libslic3r/ProjectHistoryManager.cpp`
- `src/libslic3r/ProjectHistoryManager.hpp`
- `src/slic3r/CMakeLists.txt`
- `src/slic3r/GUI/DeviceCore/DevHMS.cpp`
- `src/slic3r/GUI/DeviceCore/DevHMS.h`
- `src/slic3r/GUI/DeviceManager.cpp`
- `src/slic3r/GUI/HistorySearchStore.cpp`
- `src/slic3r/GUI/HistorySearchStore.hpp`
- `src/slic3r/GUI/LocalConfigHistory.cpp`
- `src/slic3r/GUI/LocalConfigHistory.hpp`
- `src/slic3r/GUI/PreferencesHistory.cpp`
- `src/slic3r/GUI/PreferencesHistory.hpp`
- `src/slic3r/GUI/PrinterHistory.cpp`
- `src/slic3r/GUI/PrinterHistory.hpp`
- `src/slic3r/GUI/ProjectHistoryDialog.cpp`
- `src/slic3r/GUI/ProjectHistoryDialog.hpp`
- `tests/CMakeLists.txt`
- `tests/history_search/CMakeLists.txt`
- `tests/history_search/history_search_tests.cpp`
- `tests/printer_history/README.md`
- `tests/printer_history/printer_history_tests.cpp`

## Verification

Ten standalone printer lifecycle scenarios passed using history-engine stubs. Source boundary and whitespace checks passed.

## Required continuation

Real libgit2 transitions, submitted-query tests and full native build are unverified. Add automatic preset hooks and draft record/restore adapter. Only model/preferences restore exists. The graph is a textual parent-edge list; comparison needs a proper per-key delta. Complete incident refresh, validated preference restoration, appearance/date integration and selection retention. No native UI evidence exists.

Build the reconciled candidate through the supported Windows one-click route in an isolated, pinned build tree. Keep physical printer actions and host power changes out of scope. Preserve unrelated branches. Do not delete this branch or its checkout until completed work is verified, integrated and proved on remote main.

The preservation commit deliberately uses [skip ci] to avoid starting new release workflows during the requested closeout. No hosted result, compiled application, screenshot, installer, or release is claimed by that marker. Required verification remains outstanding.

## 廣東話交接

呢條分支只係保存未完成工作，唔代表已經編譯、驗證、合併或者發佈。上面列明已做嘅檢查同未完成項目。下一次先讀返差異，再修好接駁同執行原生驗證；唔好將保存當成完成。
