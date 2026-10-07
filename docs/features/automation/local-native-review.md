# Local initial-shell review

[香港粵語](local-native-review.yue_HK.md)

`scripts/md3/local-native-review.py` validates a completed local root build and
can inspect its initial native shell. This is separate from the hosted installer
driver. It neither changes the hosted restrictions nor manufactures an installation
receipt. An authorized visible attempt exited before inspection, and the driver at
that revision did not retain the target's exit code. No startup cause is established.
The exit-observation repair has only non-window verification. Further live execution
still requires explicit authorization and a successful matching build.

## Scope and execution

Validation is the default and starts no application:

```powershell
& $Python scripts/md3/local-native-review.py `
  --producer $Producer --build-receipt $Receipt --source-commit $SourceCommit
```

After authorization, the explicit visible-desktop route is:

```powershell
& $Python scripts/md3/local-native-review.py `
  --producer $Producer --build-receipt $Receipt --source-commit $SourceCommit `
  --lowlevel-cli $LowlevelCli --execute --desktop visible
```

Add `--capture` only when a window screenshot is authorized. It captures the exact
owned HWND's client area through Lowlevel `screenshot`, never the monitor. These
variables identify existing interpreter, producer checkout, receipt and installed
Lowlevel CLI paths at runtime; they are not paths to a user's regular profile.

The visible route uses documented Lowlevel `run_command` with `command`, `shell`,
`cwd` and `timeout` arguments passed through `--json`. Its command invokes a bounded
worker in this same script. The wrapper stays hidden. A short launcher that returns
and abandons its job handle is deliberately avoided: the worker retains its unnamed
kill-on-close job for the entire inspection. The target is created suspended inside
the job through `PROC_THREAD_ATTRIBUTE_JOB_LIST`, then resumes on `WinSta0\Default`. The currently active input desktop
must already be `Default`; the route never switches desktops. Launcher paths with
shell metacharacters are rejected. The target receives only `--datadir <fresh-profile>`.

The worker deadline is 225 seconds, below Lowlevel's 270-second tool deadline and
the outer CLI's 300-second deadline. Interruption writes an owned stop marker. The
worker watchdog then exits, closing only its owned job handles. A watchdog exit or
missing worker report remains **unverified teardown**, not inferred success. Normal
teardown terminates the owned job, waits for its process and observes zero active
job processes. No process is selected or terminated by title or executable name.

Before normal owned teardown, the driver performs a zero-timeout wait on the exact
target process handle. `targetExit.status` is `exited` only when that handle is
signaled and `GetExitCodeProcess` succeeds. The report retains the unsigned 32-bit
`exitCode`, hexadecimal `exitCodeHex`, UTC observation time and
`observedBeforeTeardown: true`. A real exit code of 259 is retained after the handle
is signaled. A nonsignaled handle reports `active` without an exit code; this is a
point-in-time observation, not a claim about what happens immediately afterward.
An unavailable handle, unexpected wait result or failed query reports `unavailable`
without inventing a code. Observation failure does not prevent owned teardown.
If no session was returned, the target remains `not_observed`; an interrupted worker
without valid evidence reports `unavailable`. The wrapper's `workerExitCode` and
the termination code supplied by teardown never stand in for the target's result.
The code is diagnostic evidence, not a diagnosis of the startup cause.

The only desktop operations are window enumeration and optional exact-window
capture. The existing `send-layout-probe.py` sends a read-only layout dump request;
it does not receive a menu or navigation command. Native PID and window-class checks
filter enumeration before unrelated window details can enter the report. Ambiguous
shell windows stop inspection. Profile initialization uses English, light theme,
comfortable density, reduced motion, disabled update checking, disabled single-instance
forwarding and disabled hints. No sign-in, navigation, model import, update action,
hardware command or printing action is driven. Startup may still show a wizard or
product-owned network content; this route does not claim to prevent every startup
network request or dismiss an unexpected surface.

## Required build receipt

The current root producer writes an append-only transcript, not this JSON receipt.
The build owner must record the actual `build.bat /s` invocation and terminal result.
Do not fill missing observations from the presence of an executable. The receipt
is trusted local observer evidence, not a signed producer attestation. Keep it and
the raw transcript private or ignored, outside the public source record.

```json
{
  "schemaVersion": 1,
  "kind": "local-root-build",
  "invocationId": "actual-invocation-id",
  "entrypoint": "build.bat",
  "arguments": ["/s"],
  "sourceCommit": "40-lowercase-hex-characters",
  "sourceTree": "40-lowercase-hex-characters",
  "sourceCleanBefore": true,
  "sourceCleanAfter": true,
  "startedAtUtc": "2026-10-06T12:00:00Z",
  "finishedAtUtc": "2026-10-06T13:00:00Z",
  "exitCode": 0,
  "entrypointSha256": "64-lowercase-hex-characters",
  "transcript": {"path": "absolute-private-path", "sha256": "64-lowercase-hex-characters"},
  "payload": {
    "root": "absolute-producer-install-dir-path",
    "files": {
      "bambu-studio.exe": "64-lowercase-hex-characters",
      "BambuStudio.dll": "64-lowercase-hex-characters",
      "automation/bambu-automation.exe": "64-lowercase-hex-characters",
      "automation/build-identity.json": "64-lowercase-hex-characters"
    }
  }
}
```

Values above illustrate the schema, not a passing receipt. The source and tree must
still match the clean producer. The exact root entrypoint, transcript and four payload
files are hashed. The companion's own source and local-build identity must agree.
The validator selects the latest appended PowerShell transcript session, even if
it stopped before the producer's start or source-pin line. That session must contain
exactly one actual invocation start, the matching source pin, build-only completion
for this payload, and terminal workflow success, in that order. Their producer UTC
timestamps must be nondecreasing and inside the receipt's start/end interval.
`Write-BuildLog` records whole seconds, so the receipt start is compared at that
precision; local-time transcript header/footer values are not interpreted as UTC.
Success must be followed only by the closed transcript footer. A later invocation
with another source, a pre-pin interruption, or a freshly collected hash cannot
borrow an earlier completion. Duplicate, untimestamped or out-of-order markers,
out-of-interval times, missing terminal success and appended failure text are rejected.
Successful completion must be less than 24 hours old; future or unfinished times,
nonzero/unknown results, changed files, missing records and redirected file paths
are rejected. Provenance is rechecked before launch and after inspection. This binds
the listed files, not every external runtime component or operating-system state.

## Evidence and geometry

Each authorized run derives a new `bambu-local-review-<random>` directory under the
operating system's temporary root. It creates a new `profile/BambuStudio.conf` with
the existing checksum format and never overwrites a profile. Evidence is retained
there, not copied into tracked screenshots automatically:

- `request.json`: the bounded worker request and explicit desktop selection.
- `shell.jsonl`: native window, client, minimum/best-size and layout records.
- `shell.png`: optional raw owned-window capture.
- `review.json`: separate launch, target-exit observation, probe, screenshot and teardown results.
- `wrapper-failure.json`: an interrupted or incomplete worker, with unverified states.

A probe is accepted only after its terminal `end` record, exact PID/tag, profile tuple,
positive client geometry and finite measured DPI agree. The report preserves native
layout findings and other visible owned top-level windows; neither a complete probe
nor an initial frame proves unobstructed shell access. The route does not resize the
window or change DPI. Actual geometry is reported, not relabelled `1200x800` or minimum.
The product minimum remains `max(1000,76*em)` by `max(600,49*em)` and requires a separate
authorized sizing pass.

Screenshot production requires a positive PrintWindow result, exact target/path,
valid PNG pixels, measured client dimensions and nonuniform content. It still reports
`privacy: unreviewed` and `visualAcceptance: unverified`. Initial shell inspection does
not cover the nine remaining visual boundaries, the full language/theme/scale matrix,
embedded layout, renderer interiors, interactions, design parity or the 1,204 functional
obligations. The 56 structural reference boards are not screenshots.

## Focused verification

```powershell
& $Python -m unittest discover -s scripts/md3/tests -p test_local_native_review.py -v
```

The tests use temporary synthetic receipts, mocked native calls and synthetic PNGs.
They exercise negative provenance, source/payload drift, incomplete transcripts,
foreign/ambiguous windows, partial startup, timeout, atomic job membership at creation,
owned teardown, complete-probe checks and separate capture verdicts. They do not start
the product, use Lowlevel against a real window, validate a real build receipt or prove
Windows containment at runtime. A successful live review remains pending.

The exit-observation repair passes all 32 methods in this focused file, including
six new methods covering signaled zero/nonzero/259/high-bit exit codes, active and
unavailable states, failed liveness queries, observation before teardown, and teardown
after an observation exception. The old driver produces two missing-evidence errors
in the new inspection regression. Worker timeout tests also ensure a wrapper result
cannot become a target exit code. These checks launch no product and do not recover
the missing exit code from the earlier visible attempt.

The transcript-binding repair ran 15 focused `ReceiptTests` methods without native
execution. The old validator failed 16 assertions across the new negative cases;
the repaired validator passes all 15 methods. Tests cover later different-source
and pre-pin invocations, all four timestamp bounds, marker order, terminal closure,
older sessions, UTF-8/BOM/UTF-16 and fractional observer times. A separate read-only
compatibility check validated the existing immutable successful receipt for source
`a28944e3c14b2066ee63d14151c8aca23066d743` through the complete repaired validator.
That check launched nothing and establishes receipt compatibility, not UI acceptance.
