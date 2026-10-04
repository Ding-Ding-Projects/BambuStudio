# Continuation: Prepare scroll ownership

Status: unfinished preservation at the owner's requested closeout on 4 October 2026. No new implementation should start during this closeout.

Branch: `codex/bambu-sidebar-scroll`. Source checkpoint: `e6453379bb78c9cad7a673325651f9c635e0a8d0`. Base: `0c967a55786c07ef639a2cbefbe922b619c157d3`. The commit containing this document is the preservation tip; discover it with `git rev-parse HEAD`.

## Implemented source

Read the source diff and the dedicated feature documentation. This branch is not integrated into main and is not a verified release.

- `docs/features/prepare/scroll-ownership.md`
- `src/slic3r/GUI/ParamsPanel.cpp`
- `src/slic3r/GUI/ParamsPanel.hpp`
- `src/slic3r/GUI/Plater.cpp`
- `src/slic3r/GUI/Widgets/MD3ScrolledWindow.cpp`
- `src/slic3r/GUI/Widgets/MD3ScrolledWindow.hpp`
- `ui-md3/tests/prepare-scroll-owner.test.mjs`

## Verification

Scrollbar contracts: 10/10; sidebar-width contracts: 4/4; scroll-owner contracts: 4/4. Removing the owner relationship made the negative contract fail.

## Required continuation

Independent interaction candidate S-INT-1: wx wheel delivery may be consumed by the zero-rate inner scroller. Refute against the pinned wx source and a native wheel test. Keyboard/focus/search, last-row access, resizing and scale matrix remain unverified.

Build the reconciled candidate through the supported Windows one-click route in an isolated, pinned build tree. Keep physical printer actions and host power changes out of scope. Preserve unrelated branches. Do not delete this branch or its checkout until completed work is verified, integrated and proved on remote main.

The preservation commit deliberately uses [skip ci] to avoid starting new release workflows during the requested closeout. No hosted result, compiled application, screenshot, installer, or release is claimed by that marker. Required verification remains outstanding.

## 廣東話交接

呢條分支只係保存未完成工作，唔代表已經編譯、驗證、合併或者發佈。上面列明已做嘅檢查同未完成項目。下一次先讀返差異，再修好接駁同執行原生驗證；唔好將保存當成完成。
