# Build from source (Windows installer)

## Overview

The Windows installer offers an optional, interactive **Build from source** install source alongside
the default prebuilt install. When chosen, the installer bootstraps a developer toolchain, clones this
repository, compiles the application on the user's machine, stages the built payload, and then feeds
that payload into the **same** ownership, recovery, and uninstall flow the prebuilt path uses. The only
thing that differs between the two sources is how the payload is produced and how owned files are later
enumerated for uninstall.

Building from source is deliberately restricted to interactive setup. It is never reachable under `/S`,
and it is never executed in CI (see "CI degradation"). It exists for advanced users who want to build
locally rather than trust the prebuilt binary.

The source selection is recorded in the registry as `InstallSource` (`prebuilt` | `from-source`). The
uninstaller branches on that value; see the base installer feature doc,
[Native Windows installer](windows-native-installer.md), for the ownership and uninstall semantics that
are shared across both sources.

## Components

- `packaging/windows/BambuStudioMD3.nsi` — hosts the install-source chooser page, the non-closable
  build progress page, the async process launch, the poll timer, the exit-code branch, and the
  from-source payload copy and manifest-driven uninstall branch.
- `packaging/windows/build-from-source/Build-FromSource.ps1` — the orchestrator run asynchronously by
  the build progress page.
- `packaging/windows/build-from-source/Toolchain.ps1` — dot-sourced toolchain detect/bootstrap.
- `packaging/windows/build-from-source/Opencode.ps1` — dot-sourced opencode install, the project-local
  allow-config emitter, and the bounded repair invocation.

The three PowerShell helpers are compiled into the installer with `File`, extracted at runtime to a
private plugins directory (`$PLUGINSDIR\bfs`), and never enter the product payload, so the software bill
of materials and ownership manifest are unaffected by them.

## End-to-end flow

When the user selects Build from source, the build progress page:

1. Creates a per-run **session directory** at
   `%LOCALAPPDATA%\codingmachineedge\BambuStudioMD3-FromSource\<yyyyMMdd-HHmmss>`. It is user-writable
   and lives entirely outside the fixed install directory, so it never trips the ownership or reparse
   guards.
2. Extracts the three helpers to `$PLUGINSDIR\bfs`.
3. Launches `Build-FromSource.ps1` **asynchronously** via `kernel32::CreateProcessW` with
   `CREATE_NO_WINDOW`, passing `-SessionDir`, `-CloneUrl` (`PRODUCT_SOURCE_REPO_URL`), `-Tag`
   (`PRODUCT_SOURCE_TAG`), `-LanguageMode`, and `-PayloadOut` (`<session>\install-dir`). It stores the
   process handle for polling. If `CreateProcessW` fails, the page treats it as exit code 40 (fatal).
4. Locks the window (see "Non-closable window") and starts a ~500 ms poll timer.

`Build-FromSource.ps1` then runs this pipeline inside the session directory, dot-sourcing the two
helpers:

1. **Pre-flight** — session directory writable, at least ~40 GB free on the session drive, and a TCP
   reach test to `github.com:443` (8 s timeout). Any failure exits **30**.
2. **Toolchain bootstrap** — install Git, Node.js LTS, the Visual Studio 2022 C++ Build Tools, and
   CMake if missing (see "Toolchain bootstrap"). Failure exits **10**.
3. **Clone + checkout** — `git clone <CloneUrl> <session>\src` then `git checkout <Tag>`. Failure exits
   **11**.
4. **opencode install + config** — global `npm install -g opencode-ai`, then write the project-local
   `opencode.json` into the clone (see "opencode bootstrap and repair loop"). Install failure exits
   **12**.
5. **Build with the bounded repair loop** — compile dependencies, compile the app, and stage the
   payload with the CMake install target, each phase wrapped by the opencode repair loop (see below).
   Exhausting the repair budget exits **20**.
6. **Stage verification + manifest** — confirm `<session>\install-dir\bambu-studio.exe` exists (else
   exit **20**), then write `<session>\owned-manifest.txt`. Success exits **0**.

The build steps are the documented Windows build path: `build_win.bat -d <session>\deps -s deps`, then
`build_win.bat -s app`, then `cmake --build build --target install --config Release
-DCMAKE_INSTALL_PREFIX=<session>\install-dir`. Running the install target mirrors the CI install step so
the staged layout matches the prebuilt payload layout.

## Toolchain bootstrap

The user's consent is the install-source choice itself; individual tool installs run silently with no
per-tool prompt. For each tool the helper probes first and installs only if missing, preferring
`winget` and falling back to a pinned official vendor installer:

| Tool | Probe | winget id | Vendor fallback (silent) |
|---|---|---|---|
| Git | `git --version` | `Git.Git` | Git for Windows `Git-64-bit.exe /VERYSILENT /NORESTART /NOCANCEL /SP-` |
| Node.js LTS | `node --version` | `OpenJS.NodeJS.LTS` | `nodejs.org` MSI `msiexec /i /qn /norestart` |
| VS 2022 Build Tools (C++) | `vswhere` for the VC toolset, else `cl` | `Microsoft.VisualStudio.2022.BuildTools` with the VCTools workload, Windows 11 SDK `10.0.22000`, and the VC CMake component | `https://aka.ms/vs/17/release/vs_BuildTools.exe` with the same `--add` set, `--quiet --wait --norestart` |
| CMake | `cmake --version` | `Kitware.CMake` | `cmake.org` MSI `/qn /norestart ADD_CMAKE_TO_PATH=System` |

winget invocations use `-e --silent --accept-package-agreements --accept-source-agreements`. The
Windows 10/11 SDK is pinned to `10.0.22000` to match the `WIN10SDK_PATH` used by the CI build. CMake may
already be provided by the Visual Studio VC CMake component, in which case its own install is skipped.
After each vendor install, `PATH` is refreshed from the machine and user registry so a freshly installed
tool is visible without relaunching. Any tool that cannot be made present throws, which the orchestrator
maps to exit **10**.

## opencode bootstrap and repair loop

opencode is the automated repair assistant. It is installed with `npm install -g opencode-ai` (which
requires the Node.js LTS bootstrapped in the previous step). Before the first repair run, and again
before every subsequent run, `New-OpencodeAllowConfig` writes `<clone>\opencode.json`.

The config grants every **action** permission class `allow` and keeps two guards at `deny`:

```json
{
  "$schema": "https://opencode.ai/config.json",
  "permission": {
    "edit": "allow",
    "bash": "allow",
    "webfetch": "allow",
    "question": "deny",
    "external_directory": "deny"
  }
}
```

Because the file is project-local, the grant applies **only** to opencode sessions launched with the
clone directory as their working directory. As a best effort, the emitter also fetches the live opencode
config schema from `https://opencode.ai/config.json` (15 s timeout) and sets any additional action class
it lists to `allow`, so future action classes are covered automatically; if the network is unavailable
the baseline above stands. `question` and `external_directory` are re-forced to `deny` after any schema
pass.

Each build phase is wrapped in the repair loop. On any failing phase, the orchestrator captures the
step name and the last ~200 log lines, composes a repair prompt asking opencode to diagnose and fix the
cause by editing files in the repository and then stop (explicitly telling it not to run the build
itself), and runs one non-interactive opencode session in the clone directory. The invocation sets the
working directory to the clone, redirects stdin from `$null` so a blocked prompt can never wait on
input, and passes an auto-approve flag (`--yes`) when the installed opencode version exposes one. The
failed phase is then re-run.

The repair budget is a **cumulative cap of 5 repair-rebuild cycles across the whole build** (the attempt
counter is not reset between phases). When a phase still fails after the fifth repair cycle, the build
exits **20**. The `attempt` count and the `maxAttempts` of 5 are surfaced in the progress UI as a
"Repair N/5" caption.

## Progress protocol (plugin-free)

The PowerShell orchestrator communicates with the NSIS poller through two files in the session directory,
so no NSIS JSON plugin is required:

- `build.log` — an append-only transcript. It is seeded once with a UTF-16LE BOM and appended without a
  BOM thereafter, so the Unicode installer reads it natively.
- `status.json` — a single status object rewritten atomically (write to `status.json.tmp`, then
  `Move-Item -Force`). It is UTF-16LE with a BOM and is formatted one field per line so the NSIS reader
  can match on the leading key token without parsing JSON:

```json
{ "schema":1, "state":"running|success|failed|fatal",
  "phase":"preflight|toolchain|clone|opencode-install|deps|app|install-target|repair|stage",
  "step":"<human label>", "attempt":0, "maxAttempts":5,
  "pctHint":0, "exitCode":null, "updatedUtc":"<iso8601>" }
```

The NSIS poll timer fires about every 500 ms and: reads `status.json` to set the step label and the
"Repair N/5" caption; reads the tail of `build.log` (seeking to the last ~2000 bytes, kept at an even
byte offset for UTF-16LE) into the read-only log-tail edit and scrolls it to the bottom; keeps the
marquee bar animating; and calls `GetExitCodeProcess`. While the process reports `STILL_ACTIVE` (259) it
keeps polling; once the process has exited it stops the timer, reads the exit code, closes the handle,
and branches.

## Exit-code contract

The orchestrator's process exit code is the contract consumed by NSIS:

| Code | Meaning | Installer action |
|---|---|---|
| 0 | payload staged and manifest written | proceed to the shared ownership install of the built payload |
| 10 | toolchain bootstrap failed | fatal error page (code + log path), quit, no payload written |
| 11 | clone or checkout failed | fatal error page |
| 12 | opencode install failed | fatal error page |
| 20 | build failed after the maximum repair cycles, or the staged payload is incomplete | bounded-failure dialog offering the prebuilt fallback or leaving the window closable |
| 30 | pre-flight (network / disk / writable) failed | fatal error page |
| 40 | unexpected orchestrator error (trap), or the process could not be launched | fatal error page |

On success the page auto-advances into the shared ownership install: `File`-based extraction is skipped
and the staged payload is copied with `CopyFiles /SILENT` from `<session>\install-dir` into the fixed
install directory, and `owned-manifest.txt` is copied in as `.md3-owned-manifest.txt` (itself owned and
removed last on uninstall). All other bootstrap steps — ownership marker, recovery uninstaller,
registry registration including `InstallSource=from-source`, reparse guards, and the
`bootstrap_cleanup` -> `ready` sequencing — are the shared, unchanged flow.

On the bounded-failure code, a restrained bilingual dialog reports the log path and offers to install the
prebuilt version instead. Choosing the fallback sets `InstallMode=prebuilt` and advances into the normal
prebuilt install; declining leaves the (now closable) window so the user can cancel. On any fatal code, a
restrained bilingual dialog reports the exit code and log path, the installer sets a nonzero error level,
and it quits without writing any payload into the fixed install directory.

## Non-closable window

While a from-source build is running, the build progress page is the only non-closable window in setup,
and only between build start and build termination. On the page's `Show`, the installer hides and
disables Cancel, disables Back/Next, and removes `SC_CLOSE` from the window's system menu with
`DeleteMenu`, which greys the titlebar close button and blocks Alt+F4. The custom abort hook also refuses
to quit while the build-active flag is set.

The rationale is that the from-source build installs machine-visible developer tools and runs a long,
multi-stage compile with an automated repair loop; closing the window mid-build would leave a partially
installed toolchain and a half-finished build with no clean recovery. Locking the window keeps the run
atomic from the user's perspective until it reaches a defined terminal state.

Close is re-enabled at exactly the three terminal transitions, each restoring the default system menu
(`GetSystemMenu` revert) and re-enabling Back/Next/Cancel before showing the next or terminal page:

1. **Success (exit 0)** — enable Next and advance into the ownership install; the window is normal from
   the Finish page onward.
2. **Bounded failure (exit 20)** — re-enable close and show the fallback dialog (log path plus the
   prebuilt-instead option).
3. **Fatal error (10/11/12/30/40)** — re-enable close and show the fatal error dialog (code plus log
   path); no payload was written.

## Uninstall of a source build

Because a source build's file set can drift from the compiled prebuilt list (different toolchain DLLs,
for example), the uninstaller branches on the registry `InstallSource` value:

- `prebuilt` — the existing compiled `Delete`/`RMDir` macros, byte-for-byte unchanged.
- `from-source` — the owned manifest `.md3-owned-manifest.txt` drives removal. Each `F|` file entry
  passes a safe-relative-path check (rejecting absolute, drive-qualified, and `..` traversal paths) and
  the same reparse guard as the prebuilt path before `Delete`; a locked file sets the error flag so the
  ownership marker and registration are preserved for a retry. Each `D|` directory entry (listed
  deepest-first) is removed best-effort with `RMDir`, so unknown paths keep their non-empty parents. The
  manifest itself is removed last. A missing manifest aborts fail-closed with a nonzero error level and
  no files removed.

The fixed-ancestor reparse asserts (install root, Programs parent, install directory, Start-menu root and
product shortcut directory) run first in both branches, exactly as on the prebuilt path.

## Failure modes

- **No network, too little disk, or an unwritable session directory** — pre-flight exits 30 before any
  tool is installed or any repository is cloned.
- **A toolchain install cannot be completed** — exit 10; nothing is written to the fixed install
  directory.
- **Clone or checkout fails** (bad tag, network drop) — exit 11.
- **opencode cannot be installed** (npm failure, opencode not on PATH afterward) — exit 12; the build
  cannot self-repair without it.
- **The build never succeeds within the repair budget, or the staged payload is missing its
  executable** — exit 20; the user is offered the prebuilt fallback.
- **An unexpected orchestrator error, or the child process fails to launch** — exit 40 via the trap.
- **A partially installed toolchain remains** — the tool installs are machine-visible and are not rolled
  back on failure; a later prebuilt or from-source run reuses whatever is already present (each tool is
  probed before install).
- **Session directory retention** — the session directory (source clone, deps, build output, staged
  payload, `build.log`, `status.json`, `owned-manifest.txt`) is **not** deleted automatically. It is
  retained for diagnosis and can be large; users may remove it manually. Every fatal and bounded-failure
  dialog reports its `build.log` path.

## Security considerations

- **Machine-wide installs.** From-source installs developer tools that are visible system-wide: Git,
  Node.js LTS, the Visual Studio 2022 C++ Build Tools with the Windows 11 SDK `10.0.22000`, and CMake.
  These are not confined to the session directory and are not removed on failure or on uninstall of the
  app. The whole path runs at user level (no elevation is requested by the installer), though individual
  vendor installers may prompt for elevation of their own.
- **Network fetches from vendor URLs.** Bootstrap downloads come from pinned official sources —
  `github.com/git-for-windows`, `nodejs.org`, `aka.ms/vs`, `github.com/Kitware/CMake` — plus the npm
  registry for `opencode-ai`, the git clone from `PRODUCT_SOURCE_REPO_URL`, and a best-effort schema
  fetch from `opencode.ai/config.json`. Downloads are not independently checksum-pinned by the helper
  beyond the vendors' own transport security.
- **Why opencode runs with the repository's `opencode.json` permissions.** For the build repair to be
  fully headless, opencode must not block on approval prompts. The emitter therefore grants every action
  permission class (`edit`, `bash`, `webfetch`, and any future action class the live schema lists)
  `allow`, while keeping `question=deny` (an interaction request auto-denies instead of hanging) and
  `external_directory=deny` (opencode cannot edit files outside the clone). The config is project-local,
  so the blanket allow is scoped strictly to the cloned source directory — the combination of a
  project-local config and `external_directory=deny` is what confines the grant to the clone.
- **Residual risk (surfaced deliberately).** Within the clone directory, opencode runs with `edit`,
  `bash`, and `webfetch` all `allow` and no interactive gate. That means the repair assistant can modify
  repository files and execute commands unattended during a repair cycle. This is an intentional trade to
  keep the build headless; it is called out so the residual risk can be reduced (for example by setting
  `external_directory=allow` only if a human truly intends broader scope, or by tightening the action
  classes) without changing the mechanism.
- **Interactive-only and off in CI.** From-source is unreachable under `/S` and is never executed on CI
  runners, so none of the above network fetches or tool installs occur in automation.
- **Non-closable window.** The window is locked only during an active build (see "Non-closable window").
  It is a UX safeguard against leaving a partially installed toolchain and half-finished build behind, and
  the abort hook plus disabled controls do not otherwise change privilege or persistence; close is
  restored at every terminal state.

## Verification and CI degradation

A source build needs hours, live internet access, and machine-wide tool installs, so it cannot run on a
disposable CI runner and is never reachable under `/S`. The from-source path therefore **degrades to
compile-and-static-check only** in CI, which is intended, not a gap:

- The existing "Create Windows installer" step compiles the real `.nsi`, which proves the new
  install-source, build-progress, and error pages and their functions compile.
- The intended additional validation is a non-executing check step: parse-check each PowerShell helper
  (`[ScriptBlock]::Create` over the file contents), static greps on the `.nsi` for the install-source
  page, the build-progress functions, the silent -> prebuilt guard, the `InstallSource` write, each new
  bilingual string pair, and the `SC_CLOSE` handling, and a tiny unit invocation of the allow-config
  emitter asserting it emits `question=deny` and `external_directory=deny`.
- Because every automated installer run uses `/S`, a silent install records `InstallSource=prebuilt`,
  which confirms from-source stays off in silent mode.

The actual source build is not exercised in CI. It is validated interactively on a developer machine
that meets the pre-flight requirements. No installer or unsigned application was executed while
preparing this documentation.
