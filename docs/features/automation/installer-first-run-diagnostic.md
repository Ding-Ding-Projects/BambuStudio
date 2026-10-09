# Installer first-run diagnostic

After an interactive install, Squirrel.Windows starts the application itself, with
`--squirrel-firstrun`, while the installer is still finishing. Every other hosted install check
installs with `--silent`, which starts nothing, so that start had never been observed on a hosted
runner. `.github/workflows/diagnose-installer-first-run.yml` observes it, and the start through the
install root that the shortcuts use, on disposable hosted Windows runners. The launcher's side of
the first start is described in [App updates](../windows/app-updates.md#shortcuts-and-install-events).

## Running it

Dispatch only: Actions, **Diagnose installer first run on hosted Windows**, Run workflow, or

```
gh workflow run diagnose-installer-first-run.yml -f tag=md3-v225 -f observe_seconds=180
```

| Input | Default | Meaning |
| --- | --- | --- |
| `tag` | `md3-v225` | The published release to install; its target must be a commit |
| `observe_seconds` | `180` | How long each start is watched after its trigger finished, 30 to 420 |

Two jobs run in parallel on `windows-2025`, each limited to 30 minutes, with read-only access to the
repository and only the run's own token, which reads the release.

| Job | Install | Phases |
| --- | --- | --- |
| `interactive` | `Setup.exe` with no arguments, in a visible window, waited for up to 600 seconds | `install-firstrun`: the start Squirrel makes. Then every process under the install root is stopped, and `control-stub`: the install root's `bambu-studio.exe`, started the way a shortcut starts it |
| `silent` | `Setup.exe --silent` | `install-silent`: nothing should start, which also checks the classification. Then `first-stub`: the first start through the install root's `bambu-studio.exe` |

Both jobs install through `scripts/ci/Verify-HostedSquirrelInstall.ps1` (with `-Interactive` for the
interactive job), so the release is checked exactly as in the other hosted checks: published
digests, the `RELEASES` row, the package contract, the installed executable and the shortcuts. The
workflow credentials are cleared before anything downloaded runs. The PowerShell is in
`scripts/ci/Diagnose-InstallerFirstRun.ps1`.

## What a phase records

Every five seconds, from the start of the installation (or of the stub) until `observe_seconds`
after it finished:

- each `bambu-studio.exe`, `Update.exe` and `Setup.exe` process: process id, parent, path, command
  line, main window handle and title, and whether it responds;
- every top-level window of the application's processes, with its z-order, visibility, minimized
  state and size, and the window in the foreground with its owning process.

Process start and stop events add exact start times and exit codes, and creation events add their
command lines. On the hosted runs so far, stop events arrived only for `Update.exe` and `Setup.exe`,
so a `bambu-studio.exe` exit code is known only when a poll caught the process and held its handle.
Each phase then saves, as text:
Squirrel's logs (every `*.log` in `%LOCALAPPDATA%\SquirrelTemp` and in the install root),
`%TEMP%\bbs-launcher-trace.log`, the newest files in `%APPDATA%\BambuStudio\log`, and the
Application event-log errors and crash reports that name the application. The classification reads
that Application log text back: each Application Error entry gives the faulting process id, the
exception code, the fault offset and the faulting module, and each Windows Error Reporting crash
report gives the same without the process id. `preflight.json` records that no installation existed,
the display adapters, and the session of the job and of the console.

## Classification

Each phase writes `receipt.json` with one of these values, decided in this order. Install-event
processes (`--squirrel-install` and the other events) and Squirrel's stub are not counted as the
application. Neither is a process that was already running before the phase began, nor the child a
faulting process creates at the fault with its own command line, which is how the copy Windows Error
Reporting takes of a faulting process appears; `not_counted` in the receipt lists both, with the
reason.

| Value | Meaning |
| --- | --- |
| `started_crashed` | An application process ended with a crash exit code (an NTSTATUS error, `0xC0000000` and above, other than the launcher's own `0xFFFFFFFF`), or the phase's Application log has an Application Error or Windows Error Reporting crash entry for `bambu-studio.exe`. It wins over every other value, whatever windows were shown first |
| `started_visible` | Nothing crashed, and an application process showed a visible window and was still running at the end, or the window was the main frame (titled `<project> - Bambu Studio`); the receipt says whether it ever was the foreground window |
| `started_hidden` | An application process was still running at the end without a visible, non-minimized window |
| `started_exited` | An application process ran and ended without a crash; the receipt gives its exit code, as Windows shows it (`0xFFFFFFFF` is the launcher's own `-1`), and its lifetime, and the basis says whether it had shown a visible window, such as the untitled startup splash |
| `not_started` | No application process was seen |

For `started_crashed`, `crash` in the receipt names the faulting process id and role, the exception
code, the fault offset and the faulting module when the Application log gives them, the exit code and
lifetime, whether the process had shown a visible window before the crash
(`window_shown_before_crash`, with its titles), and the copies it made at the fault. md3-v230 is the
case this covers: its first starts showed an untitled 480 by 480 splash, and every start ended with
an access violation (`0xC0000005`) in `BambuStudio.dll`.

`application_faults` lists every crash entry of the phase, with the exit code of the process it
names when that is known. Every one of them makes the phase `started_crashed`, whatever exit code
the process had afterwards: that exit code is known only when a poll happened to catch the process,
so it describes the crash and never decides it. md3-v229 is such a case: `BambuStudio.dll` faulted
while it initialized, Windows logged the access violation, and the launcher went on to exit with
`-1` (`EXIT -1: BambuStudio.dll load failed, error=1114` in its trace). Its starts are
`started_crashed`. When a poll caught the launcher, `crash.exit_code_hex` is `0xFFFFFFFF` and the
basis says the launcher then exited with it; when none did, the basis says the exit code was not
captured.

The receipt keeps every earlier field under its name. `crash`, `application_faults` and
`not_counted` are new, and `exit` also gives `visible_window`.

The classification is the result: a job fails only when the release cannot be verified or
installed, or the install root's `bambu-studio.exe` cannot be started. The step summary lists every
phase with its basis.

## Reading the evidence

Each job uploads `installer-first-run-<job>-<run id>-<attempt>`, kept for seven days. It holds
`preflight.json`, the install check's `install-receipt.json`, and one folder per phase with
`receipt.json`, `samples.jsonl` (one poll per line), `process-events.json` and `logs/`. Nothing in it
is an image.

In launchers built with the first-run handoff, every trace line starts with the local time and the
process id, so the lines of each process can be told apart: `launcher start:` with the command line,
`squirrel event --squirrel-firstrun:` with the parent, the wait, the stub and the outcome,
`EXIT -1:` for an early exit, and `bambustu_main returned` when the application returns. A process
that ends without that last line ended inside the application. Releases built earlier
(`md3-v225` among them) write the old lines, without time or process id, and make the first start
themselves.

## Limits

- A hosted runner has no GPU, so the application uses the bundled Mesa software renderer, and
  nobody sits at its desktop. A visible window there is good evidence that the start works; it does
  not prove that a person's desktop puts the window in front. Installs on real machines remain the
  final check.
- The logs come from a fresh runner profile with no account, but they are uploaded as they are;
  review them before quoting them elsewhere.
- The crash fields are read from the English text of the Application log that a hosted runner
  writes, and the main frame is recognized by its title with the shipped display name. A runner in
  another language, or a renamed application, still gets `started_crashed` from the exit code, but
  without the fault offset and module, and its visible start needs a process still running at the
  end.
- Contract: `node --test ui-md3/tests/squirrel-install-events.test.mjs` pins the dispatch-only
  trigger, the read-only permission, the pinned actions, the time limits, the seven-day retention,
  the inputs passed through the environment, the silent default of the install check, the absence
  of screenshots, every classification value, and the order that lets a crash win. Runtime behavior
  is established only by dispatching the workflow.
