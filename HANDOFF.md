# HANDOFF — read this first

You are taking over work on **this fork of BambuStudio** (`Ding-Ding-Projects/BambuStudio`),
a Windows desktop 3D-printing slicer written in C++ with wxWidgets. This file is written
to be self-contained: it assumes you know nothing about previous sessions. Everything
below was reviewed through **2026-07-30** unless it says otherwise.

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
- No open PRs and no open branches. Two open issues: #15 is waiting for the requested secret-history policy
  choice, while #16 has a complete local implementation, green focused build/tests, and cross-host
  transport evidence. Its full GUI build and English native clipping review are also complete; the
  bilingual/live-HA/hardware/remote evidence sequence remains in §7.1.
- The whole of the previous §7 to-do list is **finished** (see §5.3). Two of its five items were
  diagnosed wrongly by the previous session; §5.3 records what was actually true.
- **2026-07-30:** `master` had not compiled since the accessibility merge, and CI was separately red
  on a stale i18n tripwire — both fixed, `md3-v80` shipped. The Prepare sidebar was cutting the
  process settings off the right edge with no scrollbar able to reach them; fixed and captured
  (§6.9). **A reported crash and a "model has no data" tab failure remain unreproduced and open —
  see §7 item 0d before claiming either is fixed.** The crash reporter turned out to be disabled
  three ways over, which is why no crash ever left evidence; that is fixed and **merged**
  (`e445d1a19`, branch CI green), so the next crash writes a stack trace to
  `<data_dir>/log/crash_*.log`. **Ask the user for that file.** No open branches.

---

## 2. Machine facts you cannot guess

These cost previous sessions hours. Do not re-derive them.

| Thing | Value |
| --- | --- |
| Repo path | Resolve from the active checkout (`git rev-parse --show-toplevel`); on the current host it is `C:\Users\cntow\Documents\GitHub\BambuStudio`. |
| Visual Studio | Visual Studio 18 Enterprise at `C:\Program Files\Microsoft Visual Studio\18\Enterprise`. |
| MSBuild | `C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe` |
| Windows SDK | The current generated tree selects **10.0.28000.0**. Do not reuse the obsolete 10.0.26100.0 pin from an earlier host image. |
| Prebuilt deps | `..\bambu-deps\build\out_deps\usr\local` for the current generated tree. Recheck the CMake cache before regenerating. |
| App logs | `%APPDATA%\BambuStudioInternal\log\studio_*.log*` |
| App config | `%APPDATA%\BambuStudioInternal\BambuStudio.conf` (e.g. `"dark_color_mode": "1"`). **Ends with a `# MD5 checksum` line.** A stale checksum only logs a warning, but **malformed JSON makes the app silently fall back to `BambuStudio.conf.bak`** — so a botched hand-edit looks exactly like "the app ignored my setting". Edit with a real JSON serializer and recompute the checksum over everything up to and including the last `}`. |
| GPU | **None.** The app needs the Mesa llvmpipe DLLs beside the Release executable to start. |
| Display | The current primary display reports **1920 x 1080**. Treat this as a point-in-time host fact and recheck before drawing layout conclusions. |
| Python | Use the repository-vendored Lowlevel MCP venv at `vendor\lowlevel-computer-use-mcp\.venv\Scripts\python.exe`; `LLCU_VENV` may override it. |

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
That hash predates the GUI accessibility wave and must not be used as evidence for it. The current
accessibility DLL metadata is recorded in §6 only after the exact final rebuild completes.

Create a temporary `.cmd` with the checkout's resolved absolute path:

```bat
@echo off
cd /d <absolute-checkout-path>
"C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe" build\src\BambuStudio_app_gui.vcxproj /p:Configuration=Release /p:Platform=x64 /m:2 /v:minimal
```

Invoke it as `cmd.exe //d //c "<absolute-temp-cmd-path>"` and capture stdout/stderr. Git Bash
rewrites bare `/p`, `/m`, and `/v` switches, so do not call MSBuild directly from Bash. Then
**always check for compiler, linker, CMake, and MSBuild errors explicitly** and verify
`build\src\Release\BambuStudio.dll` advanced; an old DLL is not a successful current build.

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

### 3.3 The narrow display found a real bug — read this before dismissing a layout as "just this host"

The bottom action bar rendered **"Slice pl"** and *no Print button at all*. That was written off
twice as an artefact of the 832 px screen. It was not.

`update_prepare_action_bar_content()` sized the canvas-alignment spacers to the **full** sidebar
width (344 px). Those spacers are **proportion-0** sizer items; the tool row is **proportion-1**.
When a row cannot fit every minimum, `wxBoxSizer` takes its degenerate branch
(`sizer.cpp:2253`): it pays the **fixed** items in full first (`:2257-2269`) and gives the
proportional ones only what is left (`:2274-2286`). So the spacer took its 344 px and the tool row
was truncated to the remainder — short by 214 px, which cascaded down three nested sizers and
reached the Slice pill as a **92 px** window and the Print pill as a **0 px** one.

Two things make this hard to see, and both misled this session:

- **It never looks like overflow.** wx truncates the straddling item and allocates **zero** to
  everything after it (`GetMinOrRemainingSize`, `sizer.cpp:2162-2190`), so every child still
  reports a rect *inside* the frame. Measuring the children and concluding "nothing overhangs, so
  nothing is clipped" is exactly the wrong inference — the starved control is simply gone.
- **A zero-width control leaves no trace.** The Print action was absent from every capture for
  hours without anyone noticing a button was missing rather than merely narrow.

Fixed in `MainFrame.cpp`: the spacers may claim only what the row does not need — cosmetic
alignment with the 3D canvas never outranks a primary action — plus a `BOOST_LOG_TRIVIAL(warning)`
when the row is still over-subscribed, so the next starved control says so instead of vanishing.
Before/after at 846 px: `docs/screenshots/md3-conversion/action-bar-{before,after}-starved-row.png`.

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
PY="$PWD/vendor/lowlevel-computer-use-mcp/.venv/Scripts/python.exe"
DRV="$PWD/.claude/skills/run-bambustudio/driver.py"

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

### 5.0.2 Session of 2026-07-28 (latest) — the UI kit stopped calling a CDN

**What was wrong.** `ui-md3/design-system/ui_kits/bambu-studio/index.html`, published at
`/app/design-system/ui_kits/bambu-studio/`, loaded React, ReactDOM and `@babel/standalone` from
unpkg and compiled its own inlined JSX in the visitor's browser on every load. Three third-party
requests and a 2.7 MB compiler on a site whose documentation opens by promising neither — and with
unpkg unreachable the page served `<div id="app"></div>` and stopped. Nothing caught it: the layout
gate's third-party assertion only ever looked at the landing page. The file's own header also
credited an "assembler" that did not exist anywhere in the tree; the `.jsx` sources and the
assembled page were kept in step by hand.

**What is now true.**

- React 18.3.1 and ReactDOM 18.3.1 UMD production builds are vendored under the kit's `vendor/`,
  with their MIT licence. No Babel ships at all.
- `ui-md3/scripts/jsx-transform.mjs` compiles the JSX at build time — a dependency-free compiler for
  the subset the kit uses, which **throws** on anything outside it rather than guessing.
- `ui-md3/scripts/assemble-ui-kit.mjs --check|--write` is the missing assembler, wired into the
  Pages workflow beside `assemble-index.mjs --check`.
- `App.jsx` now aliases `useState` to `useAppState`. Babel used to rewrite `const` to `var`, which
  hid the fact that `Components.jsx` and `App.jsx` both declared a top-level `const { useState }`.
  Compiled as real `const` in two classic scripts that share a global scope, the second is a
  `SyntaxError` that kills every script after it. The assembler now fails the build on any such
  collision.

**Evidence.** All twelve sources compile to a program **byte-for-byte identical to Babel's own
output** (verified by printing both through Babel's printer). Driving both builds through fourteen
states — the initial render, all nine workspaces, the Print-plate dialog, its dismissal, and the
version-history drawer — the compiled page **with the network cut** produced DOM identical to the
old CDN page **online**, in all fourteen. New regression gates: `assert-pages-layout.mjs` now sweeps
every published page rather than the root, and `ui-md3/tests/offline-render.test.mjs` loads the
composed site in headless Chrome with every off-site host blackholed. Both fail on the pre-fix page
and pass on this one. Full local run: 77 static cases, 11 transform cases, the 444-case runtime
suite, and the offline suite — all green.

Documentation: [`docs/features/pages/deployment-and-layout-gate.md`](docs/features/pages/deployment-and-layout-gate.md)
and the kit's own README.

### 5.0 Session of 2026-07-28 — the GitHub Pages site was rebuilt

**What you are inheriting.** `https://ding-ding-projects.github.io/BambuStudio/` is no longer a
single scrolling landing page. It is a tabbed static application built from
`ui-md3/landing.html` + `ui-md3/site/`, and it now carries the same obligations as the desktop app.
Full documentation: [`docs/features/pages/`](docs/features/pages/README.md).

| Commit | What |
| --- | --- |
| `f3ff11044` | Rebuilt the site: eight browser-style tabs, bilingual copy at five funny levels per language, the shared regex builder, the changelog viewer over 34 real releases, notifications, settings, the 1% dim sum surprise. |
| `aea1327cd` | Gated the deploy on 444 measured runtime layout cases; replaced the workflow's inline `rsync`/`python3 -m http.server` with `compose-site.mjs` and `serve.mjs`; updated `i18n.test.mjs` for the new shape (this was issue #25). |
| `2b2b7ce45` | Fixed 29 defects confirmed by a twelve-agent adversarial review of the new site. |
| `8f4dba64e` | Fixed the prototype's eight defects from issue #24: 143 icon spans made decorative, `role="switch"` on preference toggles, real dialog semantics in `app/dialogs.js`, a title bar that no longer clips its window controls, and all ten search fields wired. |
| `beeb8703a` | Restored case-sensitive regex search: `SearchField.searchFlags()` returns filter-ready flags and an empty string verbatim, so a consumer can no longer substitute `'i'` for "no flags". |
| `30d3f884e` | Title-bar collapse rules given `!important` (the prototype's inline styles beat them otherwise) and `capture-app.mjs` added. |
| `bfd87cafa` | Removed the changelog freshness gate — every release CI publishes made the committed file stale and would have blocked the next Pages deploy. It is regenerated at deploy time now, with the committed file as the fallback. |
| `b6718eac0` | Renamed the site and prototype's material vocabulary to **ink** / **Ink Dispenser** (display text only), and repaired the Cantonese the English-side rename had silently broken. |
| `65fcd2bc0` | Retook all 23 captures; `capture-app.mjs` was still querying `input[placeholder="Search filaments"]`. |
| `5340bd466` | Reworked the `release` Pages trigger, which had failed **12 times out of 12** and never once done what it claimed. Added the first test that reads the workflow. **Still unverified** — see §5.0.1. |
| `d61e4e47f` | Gave the release dispatcher its own concurrency group so it cannot cancel a deploy and then replace it with nothing. |
| `0b900b73b` | Renamed the **published MD3 UI kit** at `/app/design-system/` — 61 matching lines across eight files that three earlier terminology passes had all walked past. |
| `d9159322e` | Widened the sweep from the kit to the whole design system; the typography specimen page used real product strings as its samples. |
| `da17ee1a7` | Fixed everything a twelve-agent adversarial review found: a GitHub behaviour I had documented backwards, a guard that exempted whole lines, a sweep reading two of five published extensions, and workflow assertions that were comment-satisfiable and vacuous on CRLF. |

**Issues closed this session:** #25 (the `i18n.test.mjs` assertion that broke master — fixed before it
was filed) and #24 (the prototype's eight defects, each closed with measured evidence). #16 and #15
are untouched and remain the concurrent session's.

#### 5.0.1 All three event paths are verified — and why the run history looks otherwise

| Path | Evidence |
| --- | --- |
| `push` → deploy | **Verified.** Many green runs, most recently `da17ee1a7`. |
| `workflow_dispatch` → deploy | **Verified.** Run `30404583829`: `deploy` ran, `redeploy-on-release` **skipped**. |
| `release` → dispatch → deploy | **Verified** by `md3-v62`, tagged at `d9159322e`. |

The release path proved out like this, and it is the only release run in the repository's history
that has not failed — 17 release runs, 16 failures, 1 success:

```
00:09:57  release md3-v62 (tag at d9159322e, contains the fix) -> run 30410305947  success
            redeploy-on-release : success, 3 steps
            deploy              : skipped
            log                 : "Dispatched a master-ref Pages deploy for md3-v62."
00:10:07  workflow_dispatch at master -> run 30410314986  success (deployed)
```

**Why the Actions tab is full of red release runs anyway**, and the trap to inherit: **a `release`
event runs the workflow file as it existed at the tag's commit.** Releases tagged at commits older
than `5340bd466` run the *old* workflow and fail the old way — zero steps, ~2 seconds, no log — no
matter what `master` says. `md3-v58` (`6d1ad69de`), `md3-v59` (`2dd74cfef`), `md3-v60`
(`a00319851`) and `md3-v61` (`a90d72989`) all did exactly that *after* the fix landed. The same rule
explains the original 12-of-12: eleven releases published while `master` carried the `release:`
trigger produced zero release runs, because their tag commits predated the trigger.

So before treating a red release run as a regression, check whether its tag predates the fix:

```bash
gh release view <tag> --json targetCommitish
git merge-base --is-ancestor 5340bd466 <sha>   # exit 0 = post-fix = should have dispatched
```

**Three claims in this file were wrong earlier and are worth knowing as a pattern**, because the
same mistake recurred four times: a check that was sound about what it read, wrapped in a claim
written wider than what it read. "No user-facing filament remains" came from grepping one file;
"72/72 published files clean" came from an audit filtered to three of the seven published
extensions (it never opened the `.jsx` that was shipping the label `Filament`); the file counts in
the correction to that were estimates rather than counts. The guards now strip identifiers and
re-test the residue instead of exempting whole lines, pin real tree sizes instead of floors, and
every number is counted. The full record, including the corrections, is
[discussion #22](https://github.com/Ding-Ding-Projects/BambuStudio/discussions/22).

**Things that will bite you if you do not know them:**

- **The tab strip must not debounce with `requestAnimationFrame`.** A page that is never painted —
  a background tab, a headless capture, the deploy gate — never runs rAF callbacks, so the strip
  would stay frozen in its pre-font-load state, which is "everything overflowed". It uses a timer.
- **No `text-overflow: ellipsis` and no horizontal scroller anywhere in `ui-md3/site/`.** The
  runtime gate fails any element whose `scrollWidth` exceeds its width, and both of those hide a
  clip rather than fix it. Long strings wrap.
- **Where the prototype lives is stamped, not sniffed.** `compose-site.mjs` rewrites
  `<meta name="bambu-app-base">` to `app/` for the published tree. A `github.io` hostname test got
  the local preview wrong — which is exactly the copy the layout gate serves.
- **`ui-md3/index.html` is generated.** Edit `app/screens/*.template.html`, then run
  `node ui-md3/scripts/assemble-index.mjs --write`. `--check` runs in the Pages workflow.
- **`site/changelog.data.js` is generated**, and deliberately **not** gated on freshness. CI
  regenerates it at deploy time from the Releases API with the committed file as the fallback.
  Gating on staleness looked tidy and was a trap: every release CI publishes makes the committed
  file stale by definition, so the next Pages deploy would fail until a human regenerated it.
- **The prototype's collapse rules need `!important`.** Every element in `ui-md3/index.html`
  carries an inline `style="display:flex"`, and an inline style beats a stylesheet rule without it.
  A responsive rule that looks correct in the diff can do absolutely nothing.
- **`ui-md3/index.html` is stored with CRLF.** A search-and-replace whose pattern spans two lines
  will silently never match. Prefer single-line edits, and verify the result rather than the diff.
- **The `github-pages` environment on this fork accepts deployments only from `master`**, and a job
  gated on that environment at any other ref is rejected *before its first step* — three seconds,
  zero steps, no log, conclusion `failure`. That is what made the `release` trigger fail 12 for 12
  without anyone noticing: a `release` event runs at the **tag** ref. Anything that must deploy off
  a non-`master` ref has to dispatch a `master` run instead, which is what `redeploy-on-release`
  does. `ui-md3/tests/site.test.mjs` now asserts that shape.
- **A `release` event runs the workflow file as it existed at the TAG's commit**, not as it exists
  on `master`. This is the part that makes the run history confusing: fixing a release-triggered
  workflow does **nothing** for releases whose tags point at older commits, and they keep failing
  the old way until they drain. `md3-v58` (tagged `6d1ad69de`) and `md3-v59` both failed exactly
  that way *after* the fix landed. The same rule explains the earlier gap: eleven releases published
  while `master` carried the `release:` trigger produced zero release runs, because their tag
  commits predated it. So the fix is only proven once a release tagged at a commit **containing**
  it publishes — until then, treat it as unverified.
- **`GITHUB_TOKEN` CAN dispatch a workflow.** An earlier version of this file said the opposite;
  that was wrong. GitHub's recursive-trigger prevention explicitly exempts two events:
  `workflow_dispatch` and `repository_dispatch` "always create workflow runs", even when signed
  with `GITHUB_TOKEN`. The job needs `actions: write`, which is the real requirement. `TOKEN_GITHUB`
  is the owner PAT this repository has (`RELEASE_TOKEN` and `ORG_TOKEN` are org-convention names it
  does not define at repository scope), and it leads the chain only so a dispatch is attributed to
  the owner rather than to `github-actions[bot]`.
- **The material vocabulary is display-only.** `ink` and `Ink Dispenser` are what a user reads;
  `filamentRows`, `?view=filament`, `.bbsflmt` and the native `.po` msgids keep upstream spelling
  because bindings and file formats match on them. But `ui-md3/app/i18n.resources.js` is the
  exception that will catch you: it is keyed on the **rendered English string**, not on a msgid, so
  renaming display text without renaming its keys makes every lookup miss and fall back to English
  — silently, with nothing anywhere reporting a problem.

**How to verify the site locally** — see
[`docs/features/pages/deployment-and-layout-gate.md`](docs/features/pages/deployment-and-layout-gate.md).
The runtime suite needs Chrome or Edge and takes about three minutes.

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

### 5.4 Session of 2026-07-28 — bug + clipping sweep, and the MD3 stock-UI purge

Two audits (38 and 42 agents), four fix waves, every patch adversarially reviewed. All of it is
pushed and ancestry-proven. **The full GUI Release build is clean** and the app runs on it.

**Crash recovery had four defects, and the previously recorded diagnosis was wrong.**
`docs/features/workspace/project-version-history.md` blamed `has_restore_data()`'s `catch (...)`.
Probing Win32 directly disproved that: `OpenProcess` on a free pid returns **`NULL`** (error 87),
not `INVALID_HANDLE_VALUE`, so for a dead pid the name comes back empty, the comparison does not
match, and the `catch` is never reached. What was actually wrong:

- the sentinel guard tested the wrong value, so a null handle reached `GetModuleFileNameEx` and
  then `CloseHandle`;
- Windows **reuses freed pids**, so relaunching after a crash could hand the new instance the
  crashed one's pid — the app then compared itself against itself, concluded another instance
  held the backup, and silently offered nothing. This is the likeliest explanation for the
  2026-07-27 observation;
- `load_string_file()` sat outside the `try`, so an unreadable lock threw out of
  `has_restore_data()` into the startup handler;
- **worst:** `Plater` discarded `preserve_unsaved_backup_in_history()`'s bool, so when preserving
  failed its "stays restorable" snackbar never fired, the user read the ordinary prompt, clicked
  Cancel, and `remove_all()` ate the only copy.

> [!IMPORTANT]
> **A severity claim was withdrawn.** The sentinel bug was first written up as crashing the app
> under strict handle checking. That was reasoned, not measured — and measuring it did not support
> it: a probe ran the old and the fixed guard under `ProcessStrictHandleCheckPolicy` and **both
> survived**, as did a control that closed a garbage non-null handle, proving the policy was never
> armed. The regression test built on that probe could not fail either way and was **removed**
> rather than left green. The sentinel fix is correctness and hygiene, not a crash fix.
> `tests/libslic3r/test_crash_restore.cpp` now records which of its cases actually discriminate.

**Fourteen more native defects** were confirmed by adversarial verification and fixed: two
`FilamentScanner` use-after-frees (a stack-allocated modal dialog with a detached 180-second
thread posting `CallAfter` on a raw `this`), an invisible keyboard focus ring on every dialog's
default action (`Primary` on a `Primary` fill), ~1.33:1 snackbar contrast, `StaticBox` flooding its
own rounded corners so every pill drew as a rectangle, `CheckBox` glyphs baked at construction,
`StateColor` missing a dark pair for `Surface`, a colour picker `Fit()` before its label had text,
a resizable dialog with **no visible close control**, a non-wrapping label truncating the real
libgit2 cause, and a command palette scrolling 52px against a 53px row pitch until the selection
left the viewport entirely.

**MD3 stock-UI purge.** The parity register said all 128 gaps were done. A fresh six-lens audit
found **33 more across 26 files, three contradicting rows marked `done`**. 34 were closed across
19 files. `FanControl` was the worst: 1161 lines with **zero** `MD3::Role` references, and its fan
toggles were PNGs in a `wxStaticBitmap` — not controls — so that popup was **mouse-only** with no
role, name or state. Row `gizmo-rail-svg-icons` is now correctly marked **partial**.

**Two conversions were reverted on principle**, and both reverts matter more than the conversions:

- the Smart Home volume control kept its native `wxSlider`, because the MD3 `Slider` could not be
  reached by Tab and had no `wxAccessible`. `Slider` has since been fixed (§7 item 2);
- `2DBed`'s X/Y axis arrows went back to pure red/green. Axis colours are **exempt data**, and the
  3D gizmo still draws pure RGB, so the conversion would have desynced the 2D preview from the 3D
  scene it mirrors.

Also: the release codename roster grew from 97 to 217 Hong Kong dishes (styles 40 → 71), append-only
and enforced by `scripts/ci/Test-ReleaseCodenames.ps1` — codenames are assigned **by index**, so an
insertion renames every later release and contradicts published immutable ones. And chocolatey's
third-party downloads now retry, after a SourceForge timeout failed a whole Windows build with zero
compile errors.

### 5.5 Earlier session — how the two features above were built

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
branch:          fix/gui-accessibility-wave; 13 feature commits plus the current native repair are
                 still branch-only. origin/master remains the integration baseline until final push.
local build:     the post-key-pair focused Release GUI library compile and full
                 BambuStudio_app_gui link exit 0 with only the existing C4099/LNK4098 warnings.
                 DLL 150,811,136 bytes, 2026-07-30 00:44:51 -04:00, SHA-256
                 1EECBBFFBB5AB87AF2A90050220E3B4A93E816291F5C29DF4276078CABF22530.
runtime smoke:   exact-final-binary Lowlevel MCP verification is pending. Older intermediate captures
                 are not proof; one file named as sliced still visibly says "Not sliced".
local tests:     all three native accessibility contracts pass; DeviceWeb accessibility/behavior,
                 changed-file lint, TypeScript, Vite, owned-web/MD3, and 726 native / 184 DeviceWeb /
                 168 legacy localization checks passed earlier in this delivery branch.
open issues:     #16 (HA handover evidence pending). #15 was refused and closed as not planned because
                 it explicitly requested retaining secret material in Git history.
open PRs:        none
```

### 6.1 GUI accessibility delivery evidence (2026-07-30)

- `SwitchBoard` exposes one grouping object with two radio-button children, reports selected and
  enabled states through `wxAccessible`, and preserves the existing `1 = left` / `0 = right`
  asynchronous command contract with the real control ID and event object.
- Its minimum size is measured from both translated labels; representative Safety/Print/AMS/Status
  callers can grow instead of clipping against legacy maximum widths.
- Arrow keys and Home/End select endpoints immediately. Space/Enter/Numpad Enter arm once and commit
  only on the matching key-up; focus loss clears the armed key. This prevents OS key repeat and a
  mismatched key release from alternating the choice or emitting duplicate commands.
- The Ink Dispenser settings gear uses the shared focusable Button command event; the stale mouse
  overload that caused the full Release unresolved external has been removed.
- The maintained contracts are `native_shared_controls_accessibility_contract`,
  `native_gui_accessibility_contract`, and `native_accessibility_contract`; all three pass. The
  repaired `libslic3r_gui` project compiles and the full Release app links through MSBuild 18.7.8
  with `/m:2`.
- The exact post-key-pair DLL is **150,811,136 bytes**, timestamped
  `2026-07-30 00:44:51 -04:00`, SHA-256
  `1EECBBFFBB5AB87AF2A90050220E3B4A93E816291F5C29DF4276078CABF22530`. Lowlevel MCP captures,
  default-branch integration, remote ancestry proof, and hosted workflow/release state remain in the
  current-state block above. Do not reuse an intermediate hash or a capture whose visible state
  contradicts its filename.

### 6.2 Two machine limits that will bite you

- **`MSBuild /m` (unbounded) runs this box out of memory.** A parallel GUI build died with
  `C3859: Failed to create virtual memory for PCH` and `C1076: compiler limit: internal heap
  limit reached` — 220 of them — while agent processes were also running. `/m:2` completes.
  Neither error is a code error; do not go looking for one.
- **A full GUI build takes ~2.5 hours, so do not use it as a syntax check.** Compile a single
  file with the real settings instead:

  ```
  MSBuild build\src\slic3r\libslic3r_gui.vcxproj /t:ClCompile /p:Configuration=Release
    /p:Platform=x64 /p:SelectedFiles="<abs path>.cpp" /p:DebugInformationFormat=None
  ```

  `DebugInformationFormat=None` matters: without it two `cl.exe` racing on the shared
  `libslic3r_gui.pdb` fail with `C1041`, which looks exactly like a real error and is not.

This handoff records local implementation evidence; exact pushed revisions, hosted runs, and
release verdicts are maintained in
[issue #16](https://github.com/Ding-Ding-Projects/BambuStudio/issues/16). The repository convention
remains that completed work lands on `master` and every push builds and publishes a release. A remote
`codex/windows-reinstall-backup-20260726-174428` branch contains an explicit WIP snapshot with a
unique commit; retain it unless its work is reviewed and safely integrated—do not delete it merely
to make the branch list look tidy.

---

## 6.9 Session of 2026-07-30 — master did not compile, and the sidebar ate the process settings

**`master` had not compiled since the accessibility merge.** `SwitchButton.cpp` defined
`SwitchBoard::Accessible`, `on_key_down()` and `activateSegment()` that the header never declared —
16 errors, all in that one file. A concurrent agent pushed a fuller fix (also adding
`AcceptsFocus`/`AcceptsFocusFromKeyboard`, `MSWWindowProc`, `DoGetBestSize`) while this session was
working, so the redundant local commit was dropped and the tree reset onto theirs. Verified by a
clean local build and by CI publishing **`md3-v80`** from the identical tree.

CI was *also* red for a second, unrelated reason that never reached the compiler:
`scripts/i18n/Test-LanguageModes.ps1` pinned DeviceWeb English resources at **178** while the tree
ships **184**. The six new keys are present and translated in both locales with matching keys and
placeholders — a stale tripwire, not a resource defect. Already fixed upstream too.

> [!WARNING]
> Two `Windows build and release` runs failed at **`Test Windows release inputs`**, *before*
> `Build slicer Win`. So CI never reached the compile break at all, and a green pre-build gate is
> not evidence that the tree compiles. Check which step failed before concluding anything.

**The Prepare sidebar was cutting the process settings off at the right edge.** The full process
tree is the settings-tab layout reparented into a 344 dip sidebar; its option rows are label + value
field and neither half reflows. Measured live: the `Layer height` row lays out **1234 px wide inside
a 348 px sidebar**. The body was created `wxSHOW_SB_NEVER` for the horizontal bar with an x-scroll
rate of `0`, so the clipped values were not merely off-screen — **nothing could scroll to them**.

The header row above it had failed the same way the Print button did (§3.3): over-subscribed, so
`wxBoxSizer` paid the fixed items in full and handed **zero** to what straddled the boundary. The
`Process` title and the Compare-presets button were *absent*, not clipped. This is now the second
time that failure mode has cost this project a visible control — when a row looks cramped, measure
the children's widths before assuming everything is merely narrow.

Fixed in `Plater.cpp` / `Plater.hpp`, all verified live at 846 px on the real Release build
(`2421f9268`, plus the grow-only follow-up):

- `update_sidebar_scroll_body()` grows the **virtual width** when content genuinely cannot compress,
  and the body has a real horizontal scrollbar to grow into. Anything that *can* reflow still gets
  the client width, so the compact cards are unchanged. A re-entrancy guard was added because
  `SetVirtualSize()` can add/remove a scrollbar, resizing the client area and re-entering the helper
  through the sidebar's own `EVT_SIZE`.
- `Plater::request_sidebar_width()` widens the dock to 480 dip in Advanced mode — **weakly**: capped
  at 55% of the frame, never below the density default, `grow_only` so a sidebar you dragged wider is
  left alone, and the sash stays draggable with the dragged width persisted by the existing idle
  handler. It shrinks back only on the explicit flip to Simple.
- The width is re-asserted on the first **laid-out** size event. The `priv` ctor runs before the
  frame has a width, so a request made there clamps to the compact default; the function returns
  `false` while the frame is too small to size against, and the caller retries instead of latching.
- Advanced mode gained its own settings-search pill on the **Simple settings** bar (same
  `OptionsSearcher` and regex builder as the compact card's, whose field is hidden with the card).
- **Object manipulation now starts hidden** and appears on selection. With nothing selected it was
  twelve en dashes under a header, costing a screenful of sidebar height.

Evidence: `docs/screenshots/sidebar-process/` (before/after pairs), documented in
`docs/features/prepare/process-settings-sidebar.md`. The 3D canvas starts at **x=348** before and
**x=461** after; `Object manipulation` is absent from `press.py controls` until something is selected.

> [!IMPORTANT]
> **Two of the reported symptoms are NOT fixed and were not reproduced.** See §7 item 0d.

**Watch out — two shadowing traps in `Plater.cpp` cost two build cycles here.** `Plater::priv::priv`
takes a parameter named **`q`** that shadows the member `Plater *q`, and the AUI block declares a
local `auto &sidebar` that shadows the member `Sidebar *sidebar`. A lambda in that scope must reach
both through `this->`, or you get `C3493: cannot be implicitly captured`.

**Also worth knowing:** `press.py controls` only enumerates *labelled* children, so custom-drawn
controls (the `Global`/`Objects` `SwitchButton`, search-field placeholders) never appear — their
absence from that list is not evidence they are missing. Crop the capture instead.

---

## 7. What to do next

0d. **THE CRASH ITSELF IS STILL OPEN — start here.** It was not reproduced, so it is not fixed.
   Two of the three things reported around it *are* addressed: the app no longer refuses to reopen
   afterwards (`bbcf1630b`, below), and a crash will finally leave a stack trace once
   the crash reporter is merged and green. The crash itself has not been found. The user
   reported, in their words: *"it keeps crashing … when opening model or changing a lot of settings
   at the same time"*, *"when it crashes it refuses to open until i open it a few times"*, and
   *"switching tabs do not work and say model has no data"*.

   **Why there was never any evidence — this is the actionable finding.** The crash reporter exists
   in this tree and was switched off in *three independent ways*:
   - `SET_DEFULTER_HANDLER()` commented out in `bambustu_main()` (`src/BambuStudio.cpp`), for both
     release and internal builds;
   - `CBaseException::set_log_folder(data_dir())` commented out (`GUI_App.cpp`), so the filter had
     nowhere to write even if installed;
   - `src/BaseException.cpp` and `src/StackWalker.cpp` were **in the tree but compiled by no
     target**, so uncommenting either line alone only earns a link error.

   That is why a crash left no dump, no stack and no marker: the process simply stops mid-line,
   which is indistinguishable from being killed.

   > [!IMPORTANT]
   > **All three are now enabled and MERGED** (`e445d1a19`). The work went to a branch first
   > precisely because those legacy files had never been compiled here; branch CI run
   > `30589807507` came back **green** (built, linked, release published), so the merge rests on
   > evidence. **A crash now writes `<data_dir>/log/crash_<when>_<n>.log`** with the exception
   > code, registers, loaded modules and a call stack. It does **not** stop the crash — it makes
   > the next one diagnosable. **Ask the user for that file.**

   **What was already ruled out here (do not redo):**
   - Opening `cube.stl`, slicing, and Preview all work. **32 tab switches** across
     Prepare/Preview/Device: clean. **10 rounds** of advanced/simple flips plus every segment
     (Quality/Strength/Support/Others): clean. **12 modal open/close cycles** (`Version history`)
     with a model loaded, to fire the sidebar's 250 ms timer inside nested modal event loops:
     clean. `procdump -e -ma -w` attached throughout produced **no dump**, and both app instances
     stayed alive every time.
   - Racing the **background slicing worker** against config changes (8 rounds of Slice-plate
     followed immediately by category switches, no wait): clean.
   - Loading a **dual-filament 3MF** (`resources/calib/pressure_advance/auto_pa_line_dual.3mf`) —
     chosen because it forces a filament-count change *and* a whole-config apply at once, the
     closest thing to "changing a lot of settings at the same time": clean.
   - Testing constraint worth knowing: **this box supports only two concurrent app instances.** A
     third dies pre-log at the GL gate (`bs-out.txt` empty, no studio log, no process) because two
     llvmpipe contexts already exhaust software GL here. `driver.py open` spawns an instance, so
     with two already up it silently fails. That is a local resource limit, **not** an app defect —
     do not chase it. `single_instance` is `false` in this config, so it is not the instance check
     either.
   - **A real defect was found here by inspection and fixed** (`e897d6b3b`), though it is not
     proven to be *the* crash. `refresh_process_card()` runs off the 250 ms `m_manip_timer`, and
     every `ShowModal()` spins a nested event loop in which that timer keeps firing — so the
     function re-enters. Its `process_card_refreshing` flag (which tells the field handlers "this
     value came from the config, not the user") was set true on entry and cleared
     **unconditionally** on exit with no re-entrancy check. A nested tick therefore cleared the
     flag while the outer pass was still assigning values, so every remaining
     `SetValue()`/`SetSelection()` in that outer pass was treated as a **user edit** →
     `tab->load_config()` wrote settings nobody touched → that raised another config change → which
     scheduled another refresh. Phantom writes plus a self-feeding loop, and the window it needs is
     "a modal is open while settings are being applied" — i.e. both reported triggers. Now it bails
     out when a refresh is already in flight and restores the flag via RAII; the timer body takes
     one tick at a time.
   - **A genuine out-of-bounds crash WAS found and fixed on the model-load path** (`95fd064c0`).
     `Sidebar::on_filament_count_change()` did `choices[0]->GetDropDown().Invalidate()` whenever
     `num_physical == 1`, without checking `choices` was non-empty. With mixed filaments that
     matters: `physical_indices` collects only non-mixed slots, so `num_physical` is **0** when
     every slot is mixed; the tail of the same function then calls
     `remove_unused_filament_combos(num_physical)`, which pops `combos_filament` with **no floor of
     one** and at 0 empties it outright. The next call in with a single physical filament clears
     the `num_physical == choices.size()` early-out (0 != 1), reaches that line, and reads `[0]` of
     an empty vector — a garbage pointer dereferenced immediately by `->GetDropDown()`. In Release
     that is an access violation **on project load**, which is exactly when filament counts change.
     Now guarded with `!choices.empty()`.
     > [!WARNING]
     > **A reachability claim was withdrawn — read this before citing the fix.** It was first
     > written up as reachable through ordinary filament editing, via
     > `on_filaments_delete()` → `remove_unused_filament_combos(size - 1)` emptying the vector when
     > the last filament is deleted. **That is wrong.** `Sidebar::delete_filament()` returns early
     > on `combos_filament.size() <= 1` (Plater.cpp:5470), so the physical filaments cannot be
     > deleted down to zero. And `add_custom_filament()` appends a mixed slot at
     > `new_idx == total`, so adding mixed filaments never converts the existing physical ones —
     > `num_physical >= 1` always holds through the UI. Reasoned, not measured, and measuring it
     > did not support it. Same failure mode as the withdrawn sentinel claim in §5.3.
     >
     > What survives: this is a **latent** out-of-bounds worth guarding, not a demonstrated
     > user-facing crash. The one route not closed off is a project whose `filament_is_mixed` marks
     > every slot mixed — `check_mixed_filament_integrity()` only *flags* such slots as broken, it
     > does not refuse them, so a hand-edited or corrupt 3MF still reaches
     > `on_filament_count_change()` with `num_physical == 0`. Unverified.

     Pinned by `tests/sidebar_filament_combos/` (`a81a00fe0`), which asserts the guard, the call-site
     count, and that `remove_unused_filament_combos()` still has no floor of one. **Mutation-checked
     for real:** removing the guard fails the contract, restoring it passes.
   - Audited and clean in the same area: `update_filament_row_badges()`,
     `update_mixed_filament_list()` (all parallel-vector reads are size-checked), and the
     `combos_filament[0]` in the ctor (a `push_back` precedes it).
   - None of the above is *proven* to be the user's crash — it was found by auditing, not by
     reproducing. Do not close the crash on it; do ask for a `crash_*.log` now that one gets written.
   - No stale `wxSingleInstanceChecker` lock in `<data_dir>\cache\` and no zombie `bambu-studio.exe`
     after a run, so the "refuses to open" symptom did not reproduce either.
   - Log truncation is **not** proof of a crash: `driver.py stop` kills the process and truncates
     the buffered log identically. Six of eight older logs end mid-line for that reason. The
     2026-07-28 20:12 log that ends inside `_save_model_to_file` is a **27-second** session, which
     fits a kill far better than a crash.

   **Code paths audited and cleared (do not re-audit these):**
   - `blend_color_multi()` (`FilamentMixer.cpp:115`) and `blend_mixed_color()` (`Plater.cpp`) —
     the parallel colour/ratio vectors are bounds-guarded on both sides.
   - `has_restore_data()` (`bbs_3mf.cpp:9674`) — already hardened by the earlier session (§5.3):
     `load_string_file()` is inside the `try`, empty process names never compare equal, and pid
     reuse is handled. Not a candidate any more.
   - `Sidebar::on_filament_count_change()` / `update_mixed_filament_list()` — `physical_indices[i]`
     is bounded by `num_physical`, and the mixed-filament option reads are all size-checked.

   **The strongest untested lead:** `%APPDATA%\BambuStudioInternal\log\` holds **no logs at all from
   2026-07-29 or 07-30** despite the user hitting crashes on those days. Either they are running a
   *different* build, or it dies before the log opens (`instance_check()` runs before `wxEntry()`
   and before boost log is initialised — an early exit there produces exactly "won't open, no
   log"). **Establish which binary they actually run before anything else.** Note a release
   installer uses data dir `BambuStudio`, not `BambuStudioInternal` — and no plain `BambuStudio`
   dir exists on this host, so the reported crashes probably did not happen on this machine.

   **The "refuses to open" half IS fixed** (`bbcf1630b`, on master, CI running at session end).
   `instance_check()` discarded `send_message()`'s return value and returned `true` — terminate —
   regardless. So when the single-instance mutex is held by something that cannot answer (a process
   wedged mid-crash, one still starting, one already tearing its windows down), the launch found no
   window, handed off to nobody, and **exited anyway**. Every attempt did that until the stale
   holder released the mutex: exactly *"try it a few times and eventually it opens"*. And because
   this runs before `wxEntry()` and before boost log exists, it left **no log entry at all**, which
   also explains the missing logs above. Now the hand-off decides: if nothing took it, the instance
   starts normally and logs why. The bare blocking `SendMessage(WM_COPYDATA)` — which hangs startup
   forever against a wedged instance, same silent non-start by a different route — is now
   `SendMessageTimeout` (`SMTO_ABORTIFHUNG`, 5 s), and `l_bambu_studio_hwnd` is cleared before each
   scan so a handle from a previous enumeration can never be messaged.

   > [!NOTE]
   > That fix was pushed with **local build verification incomplete** (the branch switch invalidated
   > the CMake cache and forced a full libslic3r rebuild, which was stopped in favour of CI). It is
   > a single self-contained `.cpp` change using Win32 calls already present in that file. Confirm
   > run `30593749021` is green.

   The re-entrancy guard added to `update_sidebar_scroll_body()` is a **defensive** fix for a
   plausible recursion (`SetVirtualSize` → scrollbar → `EVT_SIZE` → repeat, which both reported
   triggers would cross). It is **not** a confirmed crash fix and must not be written up as one.

   **"Model has no data" was never located.** No such string exists in `src/` or the catalogs; the
   user is paraphrasing. Get the exact wording or a screenshot before hunting further.

0e. **The dim sum surprise, release code names, and the tabbed-README requirement are unimplemented.**
   Global memory gained sections this session that the local rules copy lacked (now synced to
   `~/.claude/rules/`): *Autonomous completion*, *Dim sum release code names*, *Landing page and
   documentation site* (tabbed README, site linked from the repo), *Sanitized instruction copy in
   every repository*, and *Build dependencies and toolchains*. Releases already carry dish code
   names (`md3-v80 — Swiss Wing 瑞士雞翼`), but the **in-app 1% startup dim sum surprise** does not
   exist, and the catalog now ships 500+ bundled PNGs in `agent-global-memory/dim-sum/` to draw from.



**Pages/site work owed (see §5.0 and §5.0.1):**

0. ~~Confirm the `release` → dispatch path~~ — **done**, proven by `md3-v62` (§5.0.1). Nothing in
   the Pages workflow is unproven now. Red release runs from tags older than `5340bd466` are the
   pre-fix queue draining, not regressions; check the tag's commit before reacting.
0b. **`README.md` and `ROADMAP.md` still describe the renamed native screens by their old labels**
   (`Filament` cards, `AMS` dialogs, the wizard's filament page). The native UI, DeviceWeb, the
   prototype, the site and the design system have all moved to ink / Ink Dispenser; those two files
   have not, so they document a UI that no longer exists. Judge each line — `FilamentPicker` is a
   class name and stays, and completed ROADMAP history should not be retroactively rewritten.
0c. The published UI kit loads React and Babel from **unpkg.com**, so it makes third-party requests
   and renders nothing if that CDN is blocked — unlike every other page on the site. A separate
   session was started for this.

Items 1 and 2 of the previous list are **done** (see §5.4). What remains, in priority order:

1. **Native captures are still owed for everything changed on 2026-07-28.** The GUI build is
   clean and the app runs, but almost none of the reskinned surfaces have been photographed.
   Highest value first, all through `.claude/skills/run-bambustudio/`:
   the **fan control popup** (the biggest single reskin, and its toggles are now real controls —
   verify they take focus), the **Slice/Print dropdowns** (`SideButton` defaults + `SideMenuPopup`
   surface), the **measurement gizmo chips in dark mode**, and the **2D bed preview in dark mode**
   — that last one has an explicit open question recorded in `2DBed.cpp:88-100`: the slab sits at
   1.05:1 against its backdrop by arithmetic, and the fix that raises it costs grid contrast. A
   capture is the only thing that settles it.
2. **Re-do the Smart Home volume slider conversion.** It was reverted because `Slider` could not
   be reached by Tab and exposed no screen-reader role. Both are fixed now (`2283f5dc8`), so the
   swap is safe — but `tests/home_assistant/home_assistant_ui_performance_contract.cmake:86`
   anchors on the literal string `m_volume->Bind(wxEVT_SLIDER`, so that contract must be updated
   in the same change or CI goes red. Keep what it is really asserting: that volume dispatch
   happens inside the debounce callback, not before the slider hook.
3. **Finish `gizmo-rail-svg-icons`** (register row now correctly marked **partial**, with the
   reasoning inline). It needs new `MaterialIcon::Glyph` codepoints, a plated-glyph entry point on
   `GLIconGlyphBridge`, and a decision on the Z-axis align/distribute tiles, which Material
   Symbols cannot express at all.
4. **Issue #24 — 8 verified `ui-md3` defects** (4 accessibility, 1 clipping, 3 search/regex).
   Left unfixed on purpose: a concurrent session owned that tree. Check whether it still does.
5. **Verify and deliver "Add my printers to Home Assistant"** (issue #16) — see §7.1.
6. **Issue #15 is waiting on the user**, not on you: whether app-data secrets are redacted,
   committed with disclosure, or encrypted. Do not start it by guessing.

### 7.0 Two traps this session paid for — do not repeat them

- **Do not edit source files while an audit or review agent is reading them.** Three verifiers
  reported findings as "refuted — this code does not exist" when what had actually happened was
  that the fix landed mid-audit. The verdicts were worthless and the time was wasted.
- **A green patch is not a correct patch.** Of eleven fixes, four passed compilation and failed
  adversarial review — one of them *introducing* a dark-mode regression while fixing a contrast
  bug (a white error link brightened to `y=1.1`, which `IM_COL32` packed with no clamp so the
  carry landed in blue and painted the underline magenta). Wave B repeated the pattern: a
  conversion added `overflow: hidden` to a compressible flex item, i.e. introduced a clipping
  defect inside a task whose entire purpose was removing them. **Review every patch, including
  the ones that compile.**

### 7.1 Item 3 in detail — Home Assistant printer handover (IMPLEMENTED; VERIFICATION PENDING)

**Current boundary:** the code, focused tests, cross-host probe, documentation, localization source,
and Windows workflow wiring are complete. The focused Release targets are built and green; the full
Release GUI build and English native 720×760/520×480 clipping review are also complete. Native
bilingual capture, live Home Assistant paths, and physical-printer success remain acceptance
conditions. Remote publication and hosted Pages are verified; Windows CI/release evidence is tracked
separately in issue #16 because this record must not predict a running job. Do not call the feature fully
runtime-verified, shipped, or issue-complete until every applicable boundary has observed evidence.

The companion
[`Ding-Ding-Projects/ha-bambulab`](https://github.com/Ding-Ding-Projects/ha-bambulab) now pins
Home Assistant 2025.1.4 plus its matching fixture package. Its canonical Ubuntu workflow passes
**93/93** in 3.36 seconds at
[run 30359258358](https://github.com/Ding-Ding-Projects/ha-bambulab/actions/runs/30359258358),
and published the tested root-content
[`v3.0.7` HACS package](https://github.com/Ding-Ding-Projects/ha-bambulab/releases/tag/v3.0.7).
Hassfest is green. The separate HACS repository validator remains red because neither fork nor
upstream declares a license; choosing terms requires owner authority and is tracked in
[companion issue #1](https://github.com/Ding-Ding-Projects/ha-bambulab/issues/1). Live Home
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
- `docs/features/pages/README.md` — the published GitHub Pages site: tabs, language modes and
  funny levels, the regex builder, the changelog viewer, and the 444-case layout gate.
- `docs/screenshots/README.md` — the screenshot matrix index.
- `docs/screenshots/pages/README.md` — captures of the published site, and how to retake them.
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
