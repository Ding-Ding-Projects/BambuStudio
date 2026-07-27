# HANDOFF — read this first

You are taking over work on **this fork of BambuStudio** (`Ding-Ding-Projects/BambuStudio`),
a Windows desktop 3D-printing slicer written in C++ with wxWidgets. This file is written
to be self-contained: it assumes you know nothing about previous sessions. Everything
below was verified on **2026-07-27** unless it says otherwise.

---

## 1. The 60-second summary

- The fork is **Windows-only**. macOS and Linux support was deleted from the tree.
- CI **works and publishes releases again**. Latest release is `md3-v14` ("Siu Yuk 燒肉").
- There is a **skill that launches and drives the app headlessly** on this machine:
  `.claude/skills/run-bambustudio/`. Use it for every "does it actually work" check.
- Open PR: **#13** (`windows-only-and-recovery-hardening`) — CI-green, ready to merge.
- One open GitHub issue: **#5** (blank README screenshots) — mostly fixed, 2 crops left.

---

## 2. Machine facts you cannot guess

These cost previous sessions hours. Do not re-derive them.

| Thing | Value |
| --- | --- |
| Repo path | `C:\Users\Administrator\Documents\GitHub\BambuStudio` |
| Visual Studio | Build Tools 17.14 installed at the **literal path `C:\Program`** (a mis-set install dir) |
| MSBuild | `C:\Program\MSBuild\Current\Bin\MSBuild.exe` |
| Windows SDK | **Must pin `PS_WINSDK=10.0.26100.0`** — SDK 10.0.28000.0 is half-installed and breaks builds with MSB8037 |
| Prebuilt deps | `deps\build\out_deps` (already built; rebuilding takes ~30 min) |
| App logs | `%APPDATA%\BambuStudioInternal\log\studio_*.log*` |
| App config | `%APPDATA%\BambuStudioInternal\BambuStudio.conf` (e.g. `"dark_color_mode": "1"`) |
| GPU | **None.** "Microsoft Basic Display Adapter". The app needs Mesa llvmpipe software GL to start at all. |
| Python | No system Python. Use `C:\Users\Administrator\Documents\GitHub\lowlevel-computer-use-mcp\.venv\Scripts\python.exe` |

**Shell gotchas on this box** (these silently produce wrong results):

- `cmd` does **not** resolve executables from the current directory. Always call `.exe`/`.cmd`
  files by absolute path, and use `cmd /c "cd /d <dir> && call C:\full\path\to\thing.bat"`.
- Git-bash `printf` eats backslashes in the **format** string: `printf 'C:\Users\...'` fails with
  "missing unicode digit for \U". Write `.cmd` files with a **quoted heredoc** (`<<'EOF'`) instead.
- Git-bash mangles `git show origin/master:path/to/file` (the colon). Use PowerShell for that,
  or `git show 'origin/master:path' -- ` quoted carefully.
- Python's `Path.write_text` converts `\n` to `\r\n` on Windows. For `.cmd` files that already
  contain `\r\n`, this produces `\r\r\n`, and the stray `\r` **poisons `set` variable values**.
  Write bytes instead.

---

## 3. How to build

### 3.1 Rebuild after editing one or a few GUI/source files (the normal case, ~10–40 min)

The real code lives in `build\src\Release\BambuStudio.dll` (~148 MB). `bambu-studio.exe` is only
a ~173 KB launcher. Rebuilding the `BambuStudio_app_gui` project pulls in everything.

```bash
cat > /c/Users/ADMINI~1/AppData/Local/Temp/msb.cmd <<'EOF'
@echo off
cd /d C:\Users\Administrator\Documents\GitHub\BambuStudio
"C:\Program\MSBuild\Current\Bin\MSBuild.exe" build\src\BambuStudio_app_gui.vcxproj /p:Configuration=Release /p:Platform=x64 /m
EOF
cmd //c "C:\Users\ADMINI~1\AppData\Local\Temp\msb.cmd" > /c/Users/ADMINI~1/AppData/Local/Temp/msb.log 2>&1
```

Then **always check for errors explicitly** — do not trust a `0` exit code alone:

```bash
grep -cE "error C[0-9]+|error LNK|CMake Error" /c/Users/ADMINI~1/AppData/Local/Temp/msb.log
grep -E "[0-9]+ Error\(s\)|Time Elapsed" /c/Users/ADMINI~1/AppData/Local/Temp/msb.log | tail -2
```

### 3.2 Three build traps that will waste your time

1. **`LNK1104: cannot open file ...BambuStudio.dll`** means **the app is still running** and
   holding the DLL. Stop it first (`driver.py stop`, see §4), then rebuild. This exact error
   ended one build in this session after a 37-minute compile.
2. **Never edit source files while a build is running** in the same tree. MSBuild's FileTracker
   gets corrupted and *later* builds exit 0 while silently skipping compiles and links. If you
   suspect it, delete the target's `.obj` and rebuild, then verify the `.obj` timestamp actually
   moved.
3. After a build, verify `BambuStudio.dll`'s timestamp advanced. MSBuild can leave the thin
   `bambu-studio.exe` stale at exit 0; the **DLL** is what matters.

---

## 4. How to RUN and DRIVE the app (this is the important part)

There is no usable interactive desktop and no GPU on this machine. You cannot "just run it".
Use the committed skill.

```
.claude/skills/run-bambustudio/
  SKILL.md        ← read this; it documents every command and every trap
  driver.py       ← the harness
  cube.stl        ← 20 mm test cube
  popovercap.py   ← for transient popovers only
```

**To press a button or menu item, use `press.py` — press by NAME, not by pixel:**

```bash
"$PY" "$D/press.py" menus                    # every menu item + its live command id
"$PY" "$D/press.py" press "Version history"  # opens File > Version history...  (verified)
"$PY" "$D/press.py" controls --filter ink    # labelled child controls
```

This solves what the previous handoff listed as the top blocker ("no known way to open a
topbar menu item programmatically"). Menu ids are `wxID_ANY` allocations that **shift between
builds** — `Version history...` was 849 in one build and 888 in the next — so `press.py`
enumerates them live and caches per frame hwnd. Never hardcode one. Two details make it work
and are easy to break: the frame must be parked at **(-183, -6)** while discovering menus (at
its normal position the owner-drawn strip opens nothing), and **ctypes silently swallows
exceptions raised inside an `EnumChildWindows` callback**, yielding an empty list instead of
an error. Both are documented in `SKILL.md`.

Quick start:

```bash
PY="C:/Users/Administrator/Documents/GitHub/lowlevel-computer-use-mcp/.venv/Scripts/python.exe"
DRV="C:/Users/Administrator/Documents/GitHub/BambuStudio/.claude/skills/run-bambustudio/driver.py"

"$PY" "$DRV" launch                                  # ~1-3 min: waits for "finished init opengl"
"$PY" "$DRV" windows                                 # find the frame (title "Untitled - BambuStudio")
"$PY" "$DRV" ss --hwnd <H> --out shot.png            # screenshot, then LOOK at it
"$PY" "$DRV" open --model .claude/skills/run-bambustudio/cube.stl
"$PY" "$DRV" ahkclick --hwnd <H> --x 975 --y 760     # click "Slice plate" (client coords)
"$PY" "$DRV" stop                                    # ALWAYS do this before rebuilding
```

**Verified working end-to-end**: launch → load cube → click Slice → the sliced Preview with the
gcode legend appears in the screenshot.

Traps the driver already handles, listed so you do not "fix" them back:

- The headless desktop **dies when its last process exits**, so the app runs in the wrapper
  `cmd`'s foreground (no `start`).
- hwnd-addressed calls **fail from the normal desktop** (`IsWindow` fails cross-desktop). Every
  such call is relayed by launching the tool *on* the headless desktop.
- **AutoHotkey never exits** without `ExitApp`, and its runtime errors open **invisible** dialogs
  on the headless desktop. Scripts must try/catch to a result file.
- `ahkclick` uses **client** coords; plain `click` uses **desktop-screen** coords (the frame sits
  at about (136, 95)). Custom wx buttons ignore plain clicks — use `ahkclick`.
- **WebView2 panes never render** in captures (Home tab, Setup Wizard body come out blank). To
  see those, render the bundled page with headless Edge instead:
  `msedge --headless=new --disable-gpu --screenshot=out.png --window-size=1200,766 file:///.../resources/web/homepage3/home.html`
- Topbar menus are **custom-drawn**: AHK `MenuSelect` fails ("unsupported menu") and clicking the
  menu labels via `ControlClick` did **not** open them in this session. Opening a menu item
  programmatically is still an **unsolved problem** — see §7.

---

## 5. What changed in this session (all of it)

### 5.1 Pushed to `master` (already live)

| Commit | What |
| --- | --- |
| `e2d2f4566` | **CI fix.** `scripts/ci/Test-WindowsNativeVisual.ps1` contained raw Cantonese text, but `scripts/ci/Test-BuildFromSourceHelpers.ps1` deliberately parses that file under Windows PowerShell 5.1's ANSI decoding *and* asserts it is byte-level ASCII. Every CI run failed with a ParseException at lines 307–308. The CJK strings are now assembled from explicit code points; output is byte-identical (verified by comparison). |
| `42f7c097b` | **CI fix.** Commit `fa0f0d6ce` added tests using `Slic3r::GUI::DeviceWeb::LatestRequestGate` but never committed the header. Every build died with C1083. Header reconstructed from the tests' contract and verified by compiling + running both test scenarios standalone. |
| `e429048f2` | Replaced blank README/wizard screenshots with genuine captures. Refs issue #5. |
| `2bc2131dc` | Added the `run-bambustudio` skill described in §4. |

**Why releases had stalled:** every run after `md3-v10` failed on the two bugs above. `md3-v11`
shipped an *old* commit simply because an older queued run finished last — the workflow's
supersession labelling was correct, nothing was mixed up. After the fixes, `md3-v12`, `v13`,
`v14`, `v15` all published. **`md3-v14` is Latest.**

### 5.2 On branch `windows-only-and-recovery-hardening` → **PR #13** (CI-green, unmerged)

| Commit | What |
| --- | --- |
| `b365c13f5` | **The fork is now Windows-only.** ~4,200 deletions. |
| `450077be1` | FadeIn hardening + documentation. |
| `477569225` | Restored a CI job that commit `b365c13f5` accidentally deleted. |

**Windows-only removal, in detail** — deleted: `BuildLinux.sh`, `BuildFedora.sh`, `BuildMac.sh`,
`DockerBuild.sh`, `DockerEntrypoint.sh`, `DockerRun.sh`, `Dockerfile`; `src/platform/osx/` and
`src/platform/unix/`; all 10 Objective-C++ `.mm` files; the macOS Homebrew deploy workflow; every
macOS/Ubuntu step in `build_bambu.yml` and `build_deps.yml`; mac/linux branches in four
CMakeLists files; the `SLIC3R_FHS` option and its generated header; GTK / webkit2gtk / GStreamer
/ Wayland / DiskArbitration wiring. `CMakeLists.txt` now **fails immediately** if configured on a
non-Windows system. `src/BambuStudio.cpp` resolves the resources dir directly instead of through
a four-way platform `#ifdef` chain.

**Deliberately NOT done:** `__APPLE__` / `__linux__` blocks *inside* shared source files remain
(~200 files). They compile out on Windows. Removing them is a separate, riskier sweep with no
functional gain. Do not start it casually.

**The FadeIn fix** (`src/slic3r/GUI/Widgets/MD3Motion.cpp`): `FadeIn` applied `WS_EX_LAYERED` with
**alpha 0** and depended entirely on a `wxTimer` to raise it. If that timer never runs, the window
stays fully transparent **while remaining modal and still consuming input** — which is exactly
what the two user reports ("Ctrl+F palette cannot be closed", "regex builder does not pop up")
look like from outside. Entrances now start at a 25% alpha floor, and if `wxTimer::Start` fails
the window jumps straight to opaque. **This is a robustness fix, not a confirmed root cause.**

**The CI job that got deleted and restored** — worth understanding, because it is how you know a
CI run is real: the job graph is
`build_all.yml` → `build_check_cache.yml` (*Check Cache*) → `build_deps.yml` (*Build Deps*) →
`build_bambu.yml` (*Build BambuStudio*). The `build_Bambu` job at the tail of `build_deps.yml` is
the link between the last two. When it was accidentally removed, the run showed *no application
build at all* and Publish failed on an installer that had never been built. **If you ever edit
these workflows, re-check that `Build BambuStudio` still appears in the job list.**

### 5.3 Uncommitted or partially verified

- **Crash-backup preservation** (`Plater::priv::preserve_unsaved_backup_in_history`, in
  `src/slic3r/GUI/Plater.cpp`, committed as part of the branch work): when the app starts and
  finds an unsaved crash backup, it now commits that backup to the local Git-backed project
  history **before** showing the "restore your last unsaved project?" prompt. This matters
  because declining the prompt runs `boost::filesystem::remove_all` on the backup directory —
  previously, Cancel was the moment the only copy of unsaved work disappeared. **Compiles;
  not yet verified live** (needs a simulated crash backup + a driven run).
  - The snapshot is staged under a real `.3mf` filename because the backup file is literally
    named `.3mf`, which has *no extension* by path rules, and the engine validates extensions on
    both the identity path and the snapshot path.
  - The commit future is `.get()`-ed because **that future carries the only error report** —
    dropping it hides failures completely.
- **ProjectHistoryDialog dark-mode fix** (`src/slic3r/GUI/ProjectHistoryDialog.cpp`): labels sat
  on light plates in dark mode. Cause: `Label`'s constructor caches its parent's background
  colour (`Label::Label` → `StaticBox::GetParentBackgroundColor`), but the dialog builds its
  layout *before* applying a theme, and `apply_theme()` only recoloured foregrounds.
  `apply_theme()` now re-seeds label backgrounds too.
  **PARTIALLY VERIFIED — captured live** via `press.py press "Version history"`; the capture is
  committed at `docs/screenshots/version-history/history-dialog-dark.png`.
  - **Fixed** (3 labels parented to the *dialog*): title, subtitle, and the safety note now sit
    on the dark surface with no plate.
  - **STILL BROKEN** (2 labels parented to a *`StaticBox` card*): `m_project_label`
    ("Project: …") and `m_status_label` ("No versions yet…") still render **black text on a
    white plate**. Note *both* their foreground and background are wrong, while the same
    `apply_theme()` call fixed the dialog-parented labels — so this is **not** the
    construction-order cache; something in the `StaticBox`-parent path (most likely MSW's
    `WM_CTLCOLORSTATIC`, which the parent answers on the child's behalf) overrides both colours.
    Do not guess-patch it: reproduce with the capture above, then find where the card's parent
    supplies the child colour.
  - The same construction-order trap probably still affects other dialogs that build layout
    before applying a theme — a sweep is worthwhile, but fix the `StaticBox` path first.

---

## 6. Current state of the world

```
branch:          windows-only-and-recovery-hardening (3 commits ahead of master)
origin/master:   2bc2131dc
PR:              #13  https://github.com/Ding-Ding-Projects/BambuStudio/pull/13   (CI green)
latest release:  md3-v14 "Siu Yuk 燒肉"
CI proof:        run 30231810171 built the PR branch tip end-to-end and published md3-v15
local build:     full Windows Release rebuild, 0 errors, BambuStudio.dll relinked 2026-07-27 00:56
open issues:     #5 only
```

**PR #13 is CI-verified and ready to merge into `master`.** The repo's convention is that work
lands on `master` and every push builds and publishes a release.

---

## 7. What to do next (ordered, with the reason)

1. **Merge PR #13 into `master` and push.** It is green; the standing rule for this repo is that
   work does not stay on a task branch.
2. **Finish the dark-mode Version-history fix.** Three of five labels are fixed and captured;
   the two on `StaticBox` cards still render black-on-white (§5.3 has the exact symptom and the
   committed evidence capture). Reproduce in one command:
   `press.py press "Version history"`, then `driver.py ss --hwnd <dialog>`.
3. **Verify crash-backup preservation live** (§5.3): create a backup directory containing a
   `.3mf`, point `app_config`'s last-backup-dir at it, launch, and confirm (a) the toast appears
   and (b) a new commit exists in the project-history repo **even if you click Cancel**.
   `press.py` can now reach the dialogs you need for this.
4. **Confirm or refute the FadeIn hypothesis** for the Ctrl+F palette and the regex builder.
   Open each (`press.py menus` lists their menu entries), screenshot, and check whether the
   window is present-but-transparent.
5. **Finish issue #5**: two crops in the screenshot matrix are still blank —
   `docs/screenshots/main-window/sidebar-prepare--gizmo-color-paint.png` and
   `--gizmo-support-paint.png`. They need a live-app recapture (the gizmo rail only renders with
   an object in the scene). Then close the issue with the evidence.

---

## 8. Rules this repo is run by (do not skip)

- **An auto-commit daemon runs on this machine.** It periodically commits *all* uncommitted
  changes in this repo and pushes `master`. Never leave half-finished work in the tree expecting
  to commit it later with a clean message. Commit deliberately and promptly.
- **Never claim a build or CI run succeeded before it reports.** Check the log, check the job
  list, and say "running" when it is running.
- **Screenshot evidence must be genuine**: from the real built binary, through the project's own
  capture harness. Never a mockup, never a different surface passed off as the fixed one.
- Commit messages are **bilingual**: concise English subject, playful Hong Kong Cantonese in the
  body.
- Every user-facing surface must follow Material Design 3, provide the three language modes
  (English / Cantonese / bilingual), use non-blocking toasts for anything informational, and
  route every search bar through the shared regex builder. See `docs/features/` for per-feature
  documentation and `.claude/skills/` for tooling.

---

## 9. Where documentation lives

- `docs/features/README.md` — index of categorized feature docs.
- `docs/features/windows/windows-only-platform.md` — the Windows-only policy in detail.
- `docs/features/workspace/project-version-history.md` — the Git-backed history feature,
  including the crash-backup preservation behaviour described in §5.3.
- `docs/screenshots/README.md` — the screenshot matrix index.
- `.claude/skills/run-bambustudio/SKILL.md` — how to run and drive the app. **Read this before
  trying to test anything in the UI.**
