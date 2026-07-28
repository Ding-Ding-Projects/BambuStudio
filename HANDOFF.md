# HANDOFF — read this first

You are taking over work on **this fork of BambuStudio** (`Ding-Ding-Projects/BambuStudio`),
a Windows desktop 3D-printing slicer written in C++ with wxWidgets. This file is written
to be self-contained: it assumes you know nothing about previous sessions. Everything
below was reviewed through **2026-07-28** unless it says otherwise.

---

## 1. The 60-second summary

- The fork is **Windows-only**. macOS and Linux support was deleted from the tree.
- CI **works and publishes releases again**. The latest verified baseline before issue #16 is
  `md3-v27`, built from `efb1689d` by green hosted run `30313911327`.
- There is a **skill that launches and drives the app headlessly** on this machine:
  `.claude/skills/run-bambustudio/`. Use it for every "does it actually work" check.
- The interactive app and GitHub Pages landing now share an eleven-image WebP showcase under
  `ui-md3/assets/showcase/`; its behavior and deployment contract are documented in
  `docs/features/design-system/generated-visual-showcase.md`.
- No open PRs. Two open issues remain: #15 is waiting for the requested secret-history policy
  choice, while #16 has a complete local implementation, green focused build/tests, and cross-host
  transport evidence. Its full GUI build and English native clipping review are also complete; the
  bilingual/live-HA/hardware/remote evidence sequence remains in §7.1.
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

The final issue #16 Release build produced a 151,299,584-byte DLL at
`2026-07-28 08:15:46 -04:00`, SHA-256
`41BB1BFC754E3184C5908E2145A93E3640D3866E59380F32EEFF7A76F418E972`.

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
branch:          master; exact remote publication state is tracked in issue #16
remote baseline: pre-issue-#16 origin/master efb1689d (tag md3-v27)
hosted baseline: md3-v27 -- run 30313911327 is GREEN; issue #16 verdict is tracked separately
local build:     full Release BambuStudio_app_gui exit 0 in 3,387 s; final incremental link 214.808 s;
                 no-change 8.544 s; DLL 151,299,584 bytes, 2026-07-28 08:15:46 -04:00,
                 SHA-256 41BB1BFC754E3184C5908E2145A93E3640D3866E59380F32EEFF7A76F418E972
local tests:     30 cases / 267 assertions; 5/5 focused CTest; 21/21 static; 156/156 browser matrix
native capture:  English 720x760 and declared-minimum 520x480 corrected captures use final 41BB1B…;
                 media-actions close-up uses preceding layout-identical EBF646… DLL
open issues:     #15 (waiting for user policy choice), #16 (implementation complete locally;
                 bilingual/live-HA/hardware/remote evidence pending)
open PRs:        none
```

This handoff records local implementation evidence; exact pushed revisions, hosted runs, and
release verdicts are maintained in
[issue #16](https://github.com/Ding-Ding-Projects/BambuStudio/issues/16). The repository convention
remains that completed work lands on `master` and every push builds and publishes a release. A remote
`codex/windows-reinstall-backup-20260726-174428` branch contains an explicit WIP snapshot with a
unique commit; retain it unless its work is reviewed and safely integrated—do not delete it merely
to make the branch list look tidy.

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
3. **Verify and deliver "Add my printers to Home Assistant"** (issue #16). Its implementation,
   focused Release build/tests, full GUI build, English native clipping review, cross-host probe,
   documentation, and CI wiring are complete locally. Read §7.1 for the remaining bilingual
   capture, live Home Assistant, hardware, and separately tracked hosted/remote evidence.
4. **The `MeshBoolean` and `FuzzySkin` gizmos have no crop** in the screenshot matrix, and `Svg`
   does not appear in the rail in this build. Neither is a defect on its own; both are worth a
   deliberate decision rather than being left implicit.
5. **Issue #15 (app-data git history) is waiting on the user**, not on you: whether secrets are
   redacted, committed with enable-time disclosure, or encrypted. The trade-offs are laid out in
   the issue comment. Do not start it by guessing.

### 7.1 Item 3 in detail — Home Assistant printer handover (IMPLEMENTED; VERIFICATION PENDING)

**Current boundary:** the code, focused tests, cross-host probe, documentation, localization source,
and Windows workflow wiring are complete. The focused Release targets are built and green; the full
Release GUI build and English native 720×760/520×480 clipping review are also complete. Native
bilingual capture, live Home Assistant paths, and physical-printer success remain acceptance
conditions. Hosted CI/release and remote-publication evidence are tracked separately in issue #16
because this local record must not predict their state. Do not call the feature fully
runtime-verified, shipped, or issue-complete until every applicable boundary has observed evidence.

The companion
[`Ding-Ding-Projects/ha-bambulab`](https://github.com/Ding-Ding-Projects/ha-bambulab) baseline
`97933ad` was CI-green with 25 tests before this pass. Its current local tree passes **89/89** tests
both in the official Home Assistant 2025.1.4 image and in a clean Python 3.12 runner. Snapshot
SHA-256 `10e2a876d0d7123f5c52450b12963b226ff1054f791db0d157fbe3321d0b2809` includes
rejected-record accounting plus the real flow-manager and nested fake-MQTT regressions. Exact
companion remote-publication and hosted-verdict evidence is tracked separately; live Home
Assistant/physical-printer verification remains pending.

**Implemented Path B — explicit service call with a Home Assistant long-lived token:**

- `SmartHomeDialog` collects accessible printers from `get_local_machinelist()` first and
  `get_user_machinelist()` second, deduped by serial. Inclusion requires access rights, serial, LAN
  address, and access code.
- The visible **Add my printers to Home Assistant** action discloses the exact fields being copied
  and states that access codes are credentials. The decision dialog uses **No** as its default.
- `HomeAssistant::add_printers()` posts each printer to
  `POST /api/services/bambu_lab/add_printer` with
  `{serial, host, access_code[, name]}` and reports processed/failed request counts on the UI thread
  (a 2xx may be an idempotent already-configured result). One import remains single-flight, but its
  requests run in four-wide waves; 32 dead endpoints therefore consume at most eight 30-second
  timeout waves instead of about 16 minutes of serial waiting.
- Progress and results use non-blocking notifications with dialog-status fallback. Failures expose
  the serial and HTTP status but never echo a response body that may contain credentials.
- Every Home Assistant bearer request now goes through `HomeAssistantTransportPolicy`: HTTPS is
  accepted; HTTP is accepted only for localhost or an explicit IPv4 loopback. Clear-text LAN HTTP,
  malformed URLs, unsupported schemes, and URL user information are rejected before networking.
  These requests also disable redirects and libcurl verbose tracing so credentials cannot be
  replayed by a redirect or printed in a protocol trace.

**Implemented Path A — temporary local discovery without a Home Assistant long-lived token:**

- **Share for discovery for 5 minutes (no Home Assistant token)** is off by default, never
  persisted, starts only through the user's toggle, and automatically turns itself off after five
  minutes.
- Every sharing window gets a fresh URL-safe capability with more than 240 random bits.
- `HomeAssistantSharingService` binds `GET /bambustudio/printers` to the RFC1918/shared IPv4 address
  on the ordinary default route (not a multicast-preferred host-only virtual adapter) and an
  operating-system-selected port, then advertises
  `_bambu-slicer._tcp.local.` PTR/SRV/TXT/A records. TXT contains `pairing` and `name`.
- The service resolves the real prefix length for that interface and answers mDNS only for usable
  senders on the exact advertised link. Network, broadcast, public, loopback, and link-local
  senders are rejected. Query admission is burst eight/refill one per second before allocation;
  replies are deduplicated, queued to eight, and paced 50 ms apart.
- The HTTP endpoint requires exactly one matching Bearer header before it asks for printer data.
  It caps header bytes, target length, timeout, concurrent sessions, response size, printer count,
  and field lengths; strips unknown fields; uses no-store/nosniff/close headers; and returns generic
  errors without reflecting sensitive values.
- Turning the toggle off, closing the dialog, reaching the five-minute expiry, or destroying the
  service closes sessions, stops serving, sends a zero-TTL mDNS goodbye, and discards the pairing
  capability.
- This path is clear-text LAN HTTP. The pairing capability is visible in mDNS to the broadcast
  domain, so the UI/docs instruct users to use a trusted LAN and keep the window brief. It is not a
  claim of encrypted transfer.

**Local build, native UI, and focused verification completed on 2026-07-28:**

- The Windows SDK 10.0.26100.0 Release build completed `home_assistant_tests` and
  `home_assistant_sharing_probe`. The test binary passed **30 cases / 267 assertions**, and all five
  `home_assistant_*` CTest entries passed.
- The full `BambuStudio_app_gui` Release build exited 0 after **3,387 seconds**; its first no-change
  rebuild exited 0 in **8.3 seconds**. After the clipping build described below, the final nonvisual
  import-scheduling and cancellation-cleanup changes compiled and linked in **214.808 seconds**; the
  final no-change rebuild exited 0 in **8.544 seconds**. The resulting DLL is 151,299,584 bytes,
  timestamped `2026-07-28 08:15:46 -04:00`, with SHA-256
  `41BB1BFC754E3184C5908E2145A93E3640D3866E59380F32EEFF7A76F418E972`.
- Lowlevel MCP headless review at 720×760 and the declared 520×480 minimum found a real clipping
  defect: `make_responsive_action` forced text actions such as **Close** and the media controls
  into 44-DIP widths. After removing that shrink, `SmartHomeDialog.cpp` rebuilt and the DLL linked
  successfully in **141 seconds**; a no-change build exited 0 in **8.0 seconds**. The two primary
  corrected captures were recaptured from the final `41BB1B…` DLL. The media-action close-up uses
  the preceding `EBF646…` DLL; the later build changes only printer-import scheduling and
  alert-light cancellation cleanup, not `SmartHomeDialog` or `MsgDialog` layout.
- Genuine before/after evidence lives under `docs/screenshots/smart-home/`:
  `dialog-720x760-before-text-action-fix.png`,
  `dialog-720x760-after-text-action-fix.png`,
  `dialog-520x480-before-text-action-fix.png`,
  `dialog-520x480-after-text-action-fix.png`, and
  `dialog-520x480-media-actions-after-fix.png`. The corrected **Close** and media actions remain
  readable at both reviewed sizes. These captures are English; bilingual native capture remains
  pending.
- On the GPU-less Mesa llvmpipe headless host, cold first-run launch took **37.427 seconds** and a
  subsequent launch took **32.339 seconds**. These are environment observations, not production
  benchmarks. The app remained responsive with no hang at about 523 MB after 13.74 minutes and
  about 549 MB after 6.26 minutes; the latter observation included three incidental Version
  History windows.
- `home_assistant_sharing_probe` runs the production service with a synthetic TEST-NET printer for
  second-host mDNS/HTTP verification without printing its token or payload. A second LAN host
  observed PTR/SRV/TXT/A, completed one authenticated bounded fetch, and observed the zero-TTL
  goodbye.
- That pass caught a real false-LAN selection: the multicast route preferred a host-only WSL
  adapter. Auto-detection now follows the ordinary default route, while the multicast flood test
  correctly sends real multicast rather than nondeterministic unicast into a shared Windows port.
- The Cantonese catalog check passed 718 entries, the static Pages/i18n/clipping suite passed 21/21,
  and the browser Pages matrix passed all 156 combinations of 13 physical widths, four zoom levels,
  and three language modes. Template assembly is synchronized. Native bilingual Smart Home capture
  remains pending.
- Performance bounds now include a 4 MiB entity-state body, at most four query domains, 512 parsed
  backend entities, 256 rendered matches, and persisted-list inspection capped at 256 segments,
  64 KiB, and 256 bytes per value with at most 32 active unique entries. Path B imports at most four
  printers concurrently, so the 32-printer cap takes no more than eight timeout waves. A failed
  two-attempt light restore retains its generation-specific recovery scene rather than deleting it;
  cancellation after scene creation but before a flash deletes the unused scene, and the focused
  regression records the exact create/delete sequence and shutdown timeout.
- `.github/workflows/build_bambu.yml` builds both targets and includes `home_assistant_tests` in the
  maintained CTest gate.
- `docs/features/windows/smart-home.md` is the user/maintainer behavior and security guide.
  `docs/features/api/home-assistant-printer-discovery.md` plus focused and master Postman
  collections document the transient endpoint.

**Do this next, in order:**

1. Capture the native bilingual Smart home dialog with `.claude/skills/run-bambustudio/` through
   Lowlevel MCP headless mode. Check the wrapped “don't show again” footer, stacked actions,
   credential disclosure, No-default confirmation, transport rejection, sharing status, keyboard
   reachability, focus, and clipping.
2. Verify Path B end-to-end against a real Home Assistant with `bambu_lab:` configured. Never place
   a real token or access code in a command line, log, screenshot, issue, Discussion, or Git.
3. Verify Home Assistant's real Path A confirmation card. Cross-host PTR/SRV/TXT/A discovery,
   authenticated fetch, and goodbye are already observed; the synthetic probe is not a real-printer
   import.
4. Verify a physical-printer success path when hardware is available; do not substitute the
   synthetic TEST-NET transport probe for that result.
5. Update `ha-bambulab/docs/verification.md` only with observed evidence. Ensure both repositories'
   remote default branches contain the exact verified commits, post the native screenshot and
   evidence to issue #16, record the observed hosted CI/release verdict there, and close #16 only
   after every acceptance condition is proven.

**Definition of done for issue #16:** the Path B button works end-to-end against a real Home
Assistant and physical printer; Path A's production service is observed across hosts and produces
the companion integration's real discovery card; native UI screenshots are posted to the issue;
both repositories are pushed; hosted checks have an honest recorded state; and no credential
appears in any retained artifact.

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
