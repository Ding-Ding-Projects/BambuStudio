# Preserved camera continuation

Historical source record; incomplete work remains pending combined acceptance.

# Continuation: Camera continuity and digital view controls

Status: unfinished preservation at the owner's requested closeout on 4 October 2026. No new implementation should start during this closeout.

Branch: `codex/bambu-camera`. Source checkpoint: `cc3dfe92e61a3b45ae29aed96bd19ff6c8bf991e`. Base: `0c967a55786c07ef639a2cbefbe922b619c157d3`. The commit containing this document is the preservation tip; discover it with `git rev-parse HEAD`.

## Implemented source

Read the source diff and the dedicated feature documentation. This branch is not integrated into main and is not a verified release.

- `docs/features/camera/README.md`
- `docs/features/camera/continuous-liveview.md`
- `src/libslic3r/AppConfig.cpp`
- `src/slic3r/GUI/CameraPlaybackPolicy.hpp`
- `src/slic3r/GUI/CameraPopup.cpp`
- `src/slic3r/GUI/CameraPopup.hpp`
- `src/slic3r/GUI/CameraViewGeometry.hpp`
- `src/slic3r/GUI/MediaPlayCtrl.cpp`
- `src/slic3r/GUI/MediaPlayCtrl.h`
- `src/slic3r/GUI/Preferences.cpp`
- `src/slic3r/GUI/StatusPanel.cpp`
- `src/slic3r/GUI/Widgets/CameraHUD.cpp`
- `src/slic3r/GUI/Widgets/CameraHUD.hpp`
- `src/slic3r/GUI/wxMediaCtrl3.cpp`
- `src/slic3r/GUI/wxMediaCtrl3.h`
- `tests/camera_playback_policy_test.cpp`
- `tests/camera_view_geometry_test.cpp`

## Verification

MSVC standalone playback assertions: 18 passed; geometry assertions: 16 passed. Deliberate retry-cap and zoom-cap mutants failed as expected.

## Required continuation

Full native build, actual rendering/input, explicit Stop, hidden/minimized continuity beyond old cutoffs, reconnect authentication states, resize/fullscreen, pointer anchors and pan bounds remain unverified.

Build the reconciled candidate through the supported Windows one-click route in an isolated, pinned build tree. Keep physical printer actions and host power changes out of scope. Preserve unrelated branches. Do not delete this branch or its checkout until completed work is verified, integrated and proved on remote main.

The preservation commit deliberately uses [skip ci] to avoid starting new release workflows during the requested closeout. No hosted result, compiled application, screenshot, installer, or release is claimed by that marker. Required verification remains outstanding.

## 廣東話交接

呢條分支只係保存未完成工作，唔代表已經編譯、驗證、合併或者發佈。上面列明已做嘅檢查同未完成項目。下一次先讀返差異，再修好接駁同執行原生驗證；唔好將保存當成完成。
