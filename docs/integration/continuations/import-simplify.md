# Preserved import-simplify continuation

Historical source record; incomplete work remains pending combined acceptance.

# Continuation: Detached import and automatic simplification

Status: unfinished preservation at the owner's requested closeout on 4 October 2026. No new implementation should start during this closeout.

Branch: `codex/bambu-import-simplify`. Source checkpoint: `33dad3b7ed58071eb4c7c8e8301b29dcfdaa71d7`. Base: `0c967a55786c07ef639a2cbefbe922b619c157d3`. The commit containing this document is the preservation tip; discover it with `git rev-parse HEAD`.

## Implemented source

Read the source diff and the dedicated feature documentation. This branch is not integrated into main and is not a verified release.

- `CLOSEOUT_PROMPT.md`
- `src/libslic3r/AppConfig.cpp`
- `src/libslic3r/CMakeLists.txt`
- `src/libslic3r/MeshSimplification.cpp`
- `src/libslic3r/MeshSimplification.hpp`
- `src/slic3r/CMakeLists.txt`
- `src/slic3r/GUI/CommandPalette.cpp`
- `src/slic3r/GUI/CommandPaletteIndex.cpp`
- `src/slic3r/GUI/Gizmos/GLGizmoSimplify.cpp`
- `src/slic3r/GUI/Jobs/ImportJob.cpp`
- `src/slic3r/GUI/Jobs/ImportJob.hpp`
- `src/slic3r/GUI/Plater.cpp`
- `src/slic3r/GUI/Plater.hpp`
- `src/slic3r/GUI/Preferences.cpp`
- `tests/libslic3r/CMakeLists.txt`
- `tests/libslic3r/test_mesh_simplification.cpp`

## Verification

Source boundary and whitespace checks passed. Six native Catch cases were authored but not run. Source is not compiled.

## Required continuation

Scene and texture publication remain synchronous/unbounded. Prove transactional cancellation and exception rollback after publication starts. Reader/hull cancellation is coarse. Archive parsing uses a modal polling owner. Extent rescaling can alter angles, volume and deviation beyond the simplifier error setting. Review that tradeoff, saved metadata and recoverable originals. Finish localization, documentation and measured native responsiveness.

Build the reconciled candidate through the supported Windows one-click route in an isolated, pinned build tree. Keep physical printer actions and host power changes out of scope. Preserve unrelated branches. Do not delete this branch or its checkout until completed work is verified, integrated and proved on remote main.

The preservation commit deliberately uses [skip ci] to avoid starting new release workflows during the requested closeout. No hosted result, compiled application, screenshot, installer, or release is claimed by that marker. Required verification remains outstanding.

## 廣東話交接

呢條分支只係保存未完成工作，唔代表已經編譯、驗證、合併或者發佈。上面列明已做嘅檢查同未完成項目。下一次先讀返差異，再修好接駁同執行原生驗證；唔好將保存當成完成。
