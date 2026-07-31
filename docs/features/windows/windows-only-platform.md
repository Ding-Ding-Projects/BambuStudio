# Windows-only platform policy

This fork builds, ships, and tests exactly one platform: **Windows x64**.
macOS and Linux support was removed from the tree rather than left unbuilt,
because unbuilt platform code silently rots and invites bug reports nobody
here can reproduce or fix.

## What was removed

| Area | Removed |
| --- | --- |
| Build scripts | `BuildLinux.sh`, `BuildFedora.sh`, `BuildMac.sh`, `DockerBuild.sh`, `DockerEntrypoint.sh`, `DockerRun.sh`, `Dockerfile` |
| Platform trees | `src/platform/osx/` (Info.plist, entitlements), `src/platform/unix/` (`.desktop` files, AppImage / Linux image builders, `fhs.hpp.in`) |
| Sources | all 10 Objective-C(++) `.mm` files (Retina helper, Mac dark mode, Mac IME, Mac instance check, Mac 3D mouse, Mac removable drives, Mac camera fullscreen, Mac media control, `MacUtils`, `Format/ModelIO`) |
| CMake | mac/linux branches in the root, `src/`, `src/slic3r/`, `src/libslic3r/`; the `SLIC3R_FHS` option and its generated header; GTK, webkit2gtk, GStreamer, Wayland, and DiskArbitration wiring |
| CI | the macOS-only Homebrew deploy workflow, and every macOS/Ubuntu step in the reusable build and deps workflows |

## Behavior

Configuring on a non-Windows system fails immediately:

```
CMake Error: BambuStudio (this fork) builds on Windows only.
Detected CMAKE_SYSTEM_NAME='Darwin'.
```

That is deliberate. A partial configure that later fails deep in a compile —
or worse, produces a binary against untested code paths — is harder to
diagnose than an explicit refusal on line one.

The resources directory is now resolved directly (`<install>/resources`)
instead of through a four-way platform `#ifdef` chain in `BambuStudio.cpp`.

## What remains

`__APPLE__` and `__linux__` blocks interleaved inside shared source files are
still present. They compile out on Windows and carry no runtime cost; removing
them touches ~200 files and risks silent breakage for no functional gain. If
that sweep is ever done, it should be mechanical, file-by-file, and verified by
a full rebuild at each step.

## Verification

The Windows Release configure and full rebuild were run locally after the
removal (`build_win.bat` tree, MSBuild `BambuStudio_app_gui.vcxproj`), and CI
builds every push on `windows-latest`. There is no other platform to verify.

## Upstream

Cross-platform builds remain available from
[Bambu Lab's upstream releases](https://github.com/bambulab/BambuStudio/releases/).
