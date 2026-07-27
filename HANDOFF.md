# HANDOFF — read this first

You are taking over work on **this fork of BambuStudio** (`Ding-Ding-Projects/BambuStudio`),
a Windows desktop 3D-printing slicer written in C++ with wxWidgets. This file is written
to be self-contained: it assumes you know nothing about previous sessions. Everything
below was verified on **2026-07-27** unless it says otherwise.

---

## 1. The 60-second summary

- The fork is **Windows-only**. macOS and Linux support was deleted from the tree.
- CI **works and publishes releases again**. Latest release is `md3-v22`
  ("Beef Brisket Noodles 牛腩麵"), built from `366917016` with a real installer attached.
- There is a **skill that launches and drives the app headlessly** on this machine:
  `.claude/skills/run-bambustudio/`. Use it for every "does it actually work" check.
- The interactive app and GitHub Pages landing now share an eleven-image WebP showcase under
  `ui-md3/assets/showcase/`; its behavior and deployment contract are documented in
  `docs/features/design-system/generated-visual-showcase.md`.
- No open PRs. Two open issues remain: #15 is waiting for the requested secret-history policy
  choice, while #16 is deliberately paused at the documented Home Assistant handoff in §7.1.
- The whole of the previous §7 to-do list is **finished** (see §5.3). Two of its five items were
  diagnosed wrongly by the previous session; §5.3 records what was actually true.

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
| App config | `%APPDATA%\BambuStudioInternal\BambuStudio.conf` (e.g. `"dark_color_mode": "1"`). **Ends with a `# MD5 checksum` line.** A stale checksum only logs a warning, but **malformed JSON makes the app silently fall back to `BambuStudio.conf.bak`** — so a botched hand-edit looks exactly like "the app ignored my setting". Edit with a real JSON serializer and recompute the checksum over everything up to and including the last `}`. |
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
- **Transient popovers die if you spawn another process on the desktop while one is open** (it
  steals focus). "Click, then list windows from another process" therefore always reports nothing,
  which reads as "the popover never opened". The click, the WinEvent catch, the repaint and the
  `PrintWindow` must happen in **one** on-desktop process — that is what `popovercap.py` is for.
  Point it at the **control's own hwnd**, not the containing panel: a raw `WM_LBUTTON*` posted to a
  panel does not fire these custom wx controls.
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

### 5.3 Session of 2026-07-27 (overnight) — §7's list is now finished

Everything the previous §7 listed is done. What it said was wrong in two places; both are
corrected below, because acting on the old text would waste hours.

| Item | Outcome |
| --- | --- |
| 1. Merge PR #13 | Already merged (`29902b4aa`) before this session. |
| 2. Dark-mode Version-history labels | **Fixed — but the diagnosis was wrong.** See below. |
| 3. Crash-backup preservation | **Verified live end-to-end**, including the Cancel branch. |
| 4. FadeIn hypothesis | **Refuted.** Both surfaces open fully opaque. |
| 5. Issue #5 blank crops | **All 14 gizmo crops were blank**, not 2. Recaptured; issue closed. |

**Item 2 — the labels were never the problem.** Pixel-sampling a live capture showed the two
"black-on-white" labels painting correctly dark (`#202127`) while the **`StaticBox` card underneath
them** painted `#F0F0F0`. Two stacked causes, both now fixed in the widget so every themed card in
the app benefits:

- `StateColor::setColorForStates()` only **updates** a state entry that already exists and returns
  `false` otherwise. `StaticBox`'s constructor seeds only `border_color`, so
  `SetBackgroundColorNormal()` was a **silent no-op on every card without an explicit
  `SetBackgroundColor()`**, and `doRender()` fell through to its `count()==0` fallback.
- That fallback fills with the plain `wxWindow` background, which `Create()` seeds once from the
  parent — the light surface for any card built before a theme is applied. `SyncWindowBackground()`
  now keeps it in step.

See `docs/features/design-system/themed-surface-colors.md`. Before/after captures are committed.

**Item 4 — refuted, and two harness traps explain the reports.** The Ctrl+F palette (642x502) and
the regex-builder popover (393x608) both open opaque and fully populated. What made them *look*
absent: a raw `WM_LBUTTON*` posted to a panel does **not** fire these custom wx controls (post to
the control's own hwnd, or use `ahkclick`), and **any process spawned on the headless desktop while
a popover is open focus-kills it** — so "click, then list windows from another process" always
reports nothing. That is exactly why `popovercap.py` exists.

The capture also caught a real defect, now fixed: every regex-builder flag row drew its text twice
(clipped ghost text inside the 44 px checkbox plus the real label), because `CheckBox` is a
`wxBitmapToggleButton` — a native MSW `BUTTON` — and `addFlag()` called `SetLabel()` on it.

**Item 5 — the scope was bigger than recorded.** All 14 gizmo crops were bare rail background
(min luminance 178, zero dark pixels), not just two. Also, two crop names are **aliases of one
gizmo each**: `color-paint` == `mmu-segment` and `support-paint` == `fdm-support`, which is why
those two looked like the only casualties. Recaptured in **light mode** (matching the rest of that
matrix) with a model loaded, since the rail only renders with an object in the scene. A sweep of
all 241 committed captures now reports zero blank images. Issue #5 closed.

**Item 3 — verified, and the fixture recipe is worth keeping.** Load a model, wait for the backup
`.3mf`, hard-kill the process, delete the stale `lock.txt`, point `app/last_backup_path` at that
directory, relaunch, click **Cancel**. Result: the backup directory is deleted and the
`Recovered unsaved project` commit survives carrying the identical 8662-byte `.3mf`. Two traps cost
real time here and are documented in `docs/features/workspace/project-version-history.md`: a
dead-pid `lock.txt` makes `has_restore_data()` return false from its `catch (...)`, and a
**hand-edited `BambuStudio.conf` with malformed JSON is silently ignored in favour of
`BambuStudio.conf.bak`** — the file ends with an MD5 checksum line, so edit it with a real JSON
serializer and recompute the checksum.

### 5.4 Earlier session — how the two features above were built

- **Crash-backup preservation** (`Plater::priv::preserve_unsaved_backup_in_history`, in
  `src/slic3r/GUI/Plater.cpp`): when the app starts and finds an unsaved crash backup, it commits
  that backup to the local Git-backed project history **before** showing the "restore your last
  unsaved project?" prompt, because declining the prompt runs
  `boost::filesystem::remove_all` on the backup directory. **Now verified live — see 5.3 item 3.**
  - The snapshot is staged under a real `.3mf` filename because the backup file is literally
    named `.3mf`, which has *no extension* by path rules, and the engine validates extensions on
    both the identity path and the snapshot path.
  - The commit future is `.get()`-ed because **that future carries the only error report** —
    dropping it hides failures completely.
- **ProjectHistoryDialog dark mode** (`src/slic3r/GUI/ProjectHistoryDialog.cpp`): `apply_theme()`
  re-seeds label backgrounds as well as foregrounds, because `Label`'s constructor caches its
  parent's background colour and the dialog builds its layout before any theme is applied. That
  fix is correct and still needed — but it was **not** what caused the remaining light plates.
  Those were the `StaticBox` bugs in 5.3 item 2. The previously suspected `WM_CTLCOLORSTATIC`
  explanation was wrong; do not go looking for it.

---

## 6. Current state of the world

```
branch:          master (no task branches, no worktrees, no stashes)
origin/master:   366917016
latest release:  md3-v22 "Beef Brisket Noodles 牛腩麵" -- built from 366917016, non-draft,
                 carries BambuStudioMD3-Setup.exe + .sha256 + CycloneDX SBOM
CI proof:        runs 30243618401 / 30244124170 / 30245103054 all GREEN, one release each
                 (md3-v20 / v21 / v22). NOTE md3-v16..v19 are NOT from this session --
                 md3-v19 targets fcc9cb6b9. A tag number is not provenance.
local build:     Windows Release, 0 errors, BambuStudio.dll relinked 2026-07-27 03:0x
open issues:     none
open PRs:        none
```

**Everything is on `master` and pushed.** The repo's convention is that work lands on `master` and
every push builds and publishes a release.

---

## 7. What to do next

The previous to-do list is finished. Nothing is blocking. In rough priority order:

1. **Sweep other dialogs for the same `StaticBox` symptom.** The two bugs in §5.3 item 2 were in
   the widget, so most surfaces are fixed for free — but any surface that sets its card colour
   through some *other* path may still be stale. The cheap check is the one that found it:
   screenshot in dark mode and **sample the pixels**, because a light plate under a correct label
   is invisible in a thumbnail.
2. **Consider whether a dead-pid `lock.txt` should really suppress crash recovery.** Today
   `has_restore_data()` returns false from its `catch (...)` when `get_process_name()` fails on the
   pid in the lock file — which is exactly the state a real crash leaves behind. This session had
   to delete the lock file to exercise recovery at all. That looks like a genuine bug in the
   recovery path, but it was **not** investigated further and is **not** confirmed; treat it as a
   lead, not a finding.
3. **Finish "Add my printers to Home Assistant"** (issue #16). This is the item mid-flight —
   read §7.1 below before touching it; it records exactly what is done, what is half-done in the
   tree, and every API you would otherwise have to rediscover.
4. **The `MeshBoolean` and `FuzzySkin` gizmos have no crop** in the screenshot matrix, and `Svg`
   does not appear in the rail in this build. Neither is a defect on its own; both are worth a
   deliberate decision rather than being left implicit.
5. **Issue #15 (app-data git history) is waiting on the user**, not on you: whether secrets are
   redacted, committed with enable-time disclosure, or encrypted. The trade-offs are laid out in
   the issue comment. Do not start it by guessing.

### 7.1 Item 3 in detail — Home Assistant printer handover (MID-FLIGHT, read all of this)

**The HA side is DONE and CI-green.** The fork
[`Ding-Ding-Projects/ha-bambulab`](https://github.com/Ding-Ding-Projects/ha-bambulab) (tip
`97933ad`, 25 tests passing) provides **two** ways in, both ending at the same config-flow import
step, both verified against a real HA 2025.1.4 in a throwaway WSL instance:

- **Path B — service call (needs an HA long-lived token):**
  `POST /api/services/bambu_lab/add_printer` with `{serial, host, access_code[, name]}`.
  Works today; the service exists with zero printers configured (registered in `async_setup`,
  cold start needs `bambu_lab:` in HA's `configuration.yaml`).
- **Path A — zeroconf discovery (NO token anywhere):** the app advertises
  `_bambu-slicer._tcp.local.` with TXT keys `pairing` (random per sharing window) and `name`,
  and serves `GET /bambustudio/printers` (`Authorization: Bearer <pairing>`) returning
  `{"printers":[{serial, host, access_code[, name]}]}`. HA raises a discovery card; the user's
  confirmation in the HA UI is the authorisation. Payload is treated as untrusted by HA
  (32 printers / 256 chars per field max). Full contract:
  `ha-bambulab/docs/unattended-printer-add.md`.

**The app side is where work stopped, mid-implementation.** State of the tree:

- **DONE, uncommitted-then-committed in this push:** `HomeAssistant::add_printers()` in
  `src/slic3r/GUI/HomeAssistant.{hpp,cpp}` — takes `std::vector<PrinterHandover>`
  (`{serial, host, access_code, name}`), POSTs each to `bambu_lab.add_printer`, reports
  `(added, per-printer errors)` back on the UI thread. Deliberately NOT fire-and-forget like the
  rest of that file: a user-initiated action must not fail silently. **Compiles-unverified — no
  build has been run since this edit. Build first, before writing more.**
- **NOT started — the UI hook for path B:** a "Add my printers to Home Assistant" action in
  `SmartHomeDialog` (`src/slic3r/GUI/SmartHomeDialog.{hpp,cpp}` — small file, reads in one pass)
  that collects printers, calls `add_printers`, and toasts the result. Non-blocking notification,
  bilingual, funny-level rules apply. It MUST tell the user plainly that printer **access codes**
  are being copied into Home Assistant at the moment it happens.
- **NOT started — path A (advertise + serve).** Two building blocks exist and were scouted:
  - `DeviceHttpServer` (`src/slic3r/GUI/DeviceWeb/DeviceHttpServer.{hpp,cpp}`, ~244 lines,
    boost::asio, binds `tcp::v4()` port 13628 on ALL interfaces) — pattern to copy or extend for
    `GET /bambustudio/printers`. Bearer-check the pairing token; never log the payload.
  - `Slic3r::Utils::Bonjour` (`src/slic3r/Utils/Bonjour.{hpp,cpp}`) is **browse-only** — it can
    look up services but CANNOT advertise one. An mDNS responder must be written (answer
    PTR/SRV/TXT/A queries for `_bambu-slicer._tcp.local.` on 224.0.0.251:5353) or vendored.
    This is the only genuinely new machinery path A needs.
  - Sharing must be OFF by default, ON only while the user has it on, fresh `pairing` token per
    window, stop advertising AND serving when the window closes.

**Where printers come from (scouted, use these, do not re-derive):**

- `DeviceManager` definition is in `src/slic3r/GUI/DeviceCore/DevManager.h` (NOT DeviceManager.hpp):
  `get_local_machinelist()` / `get_user_machinelist()` → `std::map<std::string, MachineObject*>`
  keyed by dev_id (= serial).
- Per machine (`src/slic3r/GUI/DeviceManager.hpp`): `get_dev_ip()` (:187), `get_dev_name()` (:184),
  `get_access_code()` (:213), `has_access_right()` (:212). dev_id is the serial.
- Reach it via `wxGetApp().getDeviceManager()` (verify exact accessor name in GUI_App.hpp).

**Traps already hit on the HA side (do not re-hit):**

- HA test envs miss packages a real HA fetches at runtime. The full set for the fork's tests:
  `ha-ffmpeg home-assistant-frontend paho-mqtt zeroconf aiofiles async_upnp_client beautifulsoup4`.
  Three CI runs went red on this before the recipe was rebuilt in a clean WSL container.
- The permission classifier blocked one large Edit into `config_flow.py`; splitting the code into
  its own module (`slicer_pairing.py`) went through fine and was better anyway.
- Verify in a throwaway WSL Ubuntu 24.04 (`wsl_create_temp` with the ubuntu-base rootfs URL —
  Alpine ships Python 3.14, which HA rejects). Destroy it after.

**Definition of done for issue #16:** path B button working end-to-end against a real HA (the
throwaway-WSL recipe in `ha-bambulab/docs/verification.md` reproduces one in ~10 min), path A
advertising + serving verified with the fork's discovery card actually appearing, screenshots of
the new UI surface posted to the issue, and the fork's `docs/verification.md` updated to retire
its "never run against a real broadcast" caveat.

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

---

## 10. One-click local Windows installer build

`OneClickBuildInstaller.cmd` now provides the local bootstrap → dependencies → Release app →
payload → verified Mesa fallback → SBOM → NSIS → SHA-256 path. Its implementation is
`scripts/windows/Invoke-OneClickBuild.ps1`, its static contract test is
`scripts/ci/Test-OneClickBuild.ps1`, and its operator guide is
`docs/features/releases/windows-one-click-build.md`. The installer remains unsigned and is launched
only when the caller explicitly supplies `-Install`.
