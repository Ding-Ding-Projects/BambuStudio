# Windows CI and release supply chain

## Trigger and publication policy

`.github/workflows/build_all.yml` is the fork's Windows build and publication workflow. Every
branch push and manual dispatch builds the Windows candidate; a pull request targeting `main` runs
the build without publishing. A lightweight path-classification job remains informational, so
documentation-only pushes are not silently skipped. Branch-only push filters and an explicit tag
guard prevent release tags from recursively starting another build.

Every successful non-pull-request branch-push or manual-dispatch run publishes one uniquely tagged,
non-draft release. Tags include the application version and workflow run number. A rerun converges on
the same tag instead of creating a duplicate. The release job validates the exact Squirrel assets,
source-commit metadata, checksum, an empty PE security directory (unsigned Setup.exe), feed index, full package, SBOM, and
GitHub asset digests before publishing the draft. Apart from the draft that holds the build cache,
which is never published (see [Build cache](#build-cache)), the build job creates no cache
prerelease or other secondary GitHub Release.

Release jobs run one at a time, and each decides "latest" immediately before publication. A
default-branch build becomes latest when its commit is newer than the commit of the release that is
latest now (or is that same commit rebuilt). An older build that finishes late is published with
"(superseded main build)" in its title and stays non-latest, and builds of other refs stay
non-latest. The branch may have moved on while the build ran; the latest release, and the update
feed installed copies read from it, still move forward.

The release concurrency group uses `queue: max` with `cancel-in-progress: false`: one release job
runs while up to 100 jobs wait. New arrivals beyond that limit are canceled. Waiting jobs are
processed in FIFO order by when they entered the concurrency queue, not by workflow dispatch or
source-commit order. The existing latest-release comparison remains necessary. See the
[GitHub concurrency documentation](https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/control-workflow-concurrency).

Previously, the default single pending slot could cancel a waiting release even though
`cancel-in-progress: false` protected the running job. Run `37088514258` built and uploaded its
installer successfully, but publication job `111113206485` was canceled before any steps ran.
Its annotation reported: "Canceling since a higher priority waiting request for
windows-release-Ding-Ding-Projects/BambuStudio exists".

For an affected historical run, first confirm its build succeeded, its installer artifact has not
expired, and no release publication attempt is already active. Retry only the unsuccessful jobs
with `gh run rerun RUN_ID --failed`, or select the canceled publication job explicitly with
`gh run rerun --job JOB_ID`. Do not rerun the successful build merely to recover publication.
The release job downloads that run's existing installer artifact and retains its original source
identity and tag-allocation/idempotence checks. Re-runs use the original workflow revision, so
this queue change does not protect a historical re-run's old single pending slot. Coordinate its
retry when the release queue is clear, then verify the resulting release, source and asset hashes,
and terminal job state. GitHub permits re-runs within 30 days of the initial run, with at most 50
attempts; artifact availability is a separate requirement. See
[Re-running workflows and jobs](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/re-run-workflows-and-jobs).

## Windows build and package boundary

The reusable build resolves or rebuilds the dependency cache, configures and installs the production
native Release payload, adds the hash-pinned Mesa software-OpenGL fallback, generates a CycloneDX 1.6
inventory, and packages the payload with the committed `scripts/windows/Invoke-SquirrelPackage.ps1`.
Squirrel.Windows 2.0.1 is downloaded from the official NuGet flat-container URL only when it is not
already cached, and its package SHA-256 is checked before extraction.

The current workflow deliberately keeps correctness and UI evidence checks as local release-operator
checks rather than Actions test jobs. The committed local checks remain available and are run before a
manual release or before accepting a candidate build. A workflow build still fails on compiler,
dependency, SBOM, or Squirrel packaging failures.

## Build cache

A hosted Windows build compiled nearly every source every time. sccache wraps the compiler, but it
cannot cache a compile that uses the precompiled header, which nearly every source does: in run
36631880242 (`bb78abee1`) it saw 795 compile requests and cached 99, while 696 were non-cacheable
(`/Fp` 693, `/Yc` 3). The "Build slicer Win" step took 72 minutes of that run's 80-minute build
job. The build therefore reuses the whole Ninja build tree of the latest `main` build, precompiled
headers included, and Ninja compiles only what changed since.

**Where it lives.** The tree is kept in the draft release `build-cache-windows`, in 7-Zip volumes
of at most 1,500,000,000 bytes each (GitHub accepts release assets up to 2 GiB). A draft never fires
release events, never becomes the latest release and keeps its assets replaceable, while the
published releases here are immutable. It is never published, and `Save-BuildCache.ps1` refuses to
write to it if it ever is. `windows-build-latest.json` names the current set. Each set is
`windows-build-<commit>.7z.001`, `.002`, ... plus `windows-build-<commit>.json`, a manifest with
the commit, run number, key, tree size, and every part's name, size, and SHA-256. The steps read
and write the draft with the owner token secret `TOKEN_GITHUB`, which `build_all.yml`,
`build_check_cache.yml`, and `build_deps.yml` pass down with `secrets: inherit`.

**Restore, before the compile.** `scripts/ci/Restore-BuildCache.ps1` uses the tree only when its key
equals this run's: MSVC toolset (`VCToolsVersion`), Windows SDK, CMake and Ninja versions, the
dependency cache key, the workspace path, and the script's layout version. It checks the free disk
space, downloads the parts, and checks every size and SHA-256 before extracting. A checkout gives
every file the current time, which would make Ninja rebuild everything. So every tracked file is
set to 2020-01-01, then every file that differs between the cached commit and the checkout (the
working tree, so a file an earlier step edited counts) is set to now. Objects keep their build
times, so exactly the changed files and what includes them are rebuilt. 2020 rather than an
earlier date: Ninja on Windows counts time from about 2001. The device page bundle is built into
the source tree, which a checkout lacks, so its stamp is removed and the bundle rebuilt.

**Save, after a `main` build.** Only a successful push build of `main` saves, so every build starts
from `main`'s tree. `scripts/ci/Save-BuildCache.ps1` starts in the background while the payload is
packaged. It is started hidden and writes its own log: started with redirected output, it would hold
the step's output open after the step ended (measured locally), and the runner stops the processes
still holding a finished step's output. It archives the tree with fast LZMA2 compression (`-mx=1`), leaving out the `resources`
junction, debug databases, and the device page's package store. It uploads the parts and the
manifest, reads every part's size back from GitHub, and moves `windows-build-latest.json` only when
no newer run already points it at its own set. It keeps the newest three sets and removes parts
without a manifest once they are two hours old, so a run still uploading keeps its own. It refuses
to start when the drive lacks the tree size plus 10 GB. The job's last step waits up to 30 minutes
for it and prints its log.

**It can make a build faster, never make it fail.** A missing token (a pull request from a fork),
no cached set, a different key, a damaged or missing part, too little disk space, or a tree already
in place means a build from scratch, as before, with the reason in a warning. The restore removes
only a tree it extracted itself. When a restored tree fails to configure, the build step deletes it
and configures from scratch. A save that fails is a warning. To force a build from scratch, put
`[cold build]` in the pushed commit's message. It skips the restore, and a `main` build still saves
a fresh tree. Changing the layout version in both scripts discards every existing set.

The build-time stamp (`SLIC3R_BUILD_TIME`, `SLIC3R_BUILD_TIME_UTC`) now lives in
`libslic3r_build_time.h`, which only the log, the About dialog, and the splash date include. In
`libslic3r_version.h`, which nearly every source and the precompiled header include, a value that
changes with every configure made every object out of date on every build, cache or not.

Checked locally before the first hosted run. A run of both scripts against a stand-in `gh` backed by
a folder, with the real 7-Zip and a scratch repository of four commits, passed 34 checks: parts at
most the limit, a warm restore with exact timestamps, the pointer never moving back, pruning to three
sets, and every fallback leaving no tree behind. A Ninja file whose steps only write files and
print an MSVC include note (no compiler) confirmed three things after the 7-Zip round trip: object
times come back exact to 100 ns, nothing is rebuilt when nothing changed, and a changed source or
header rebuilds only its objects and the link. A stand-in for the runner, reading a step's output
to the end, showed why the save starts hidden: a child started with redirected output held that
output open until it finished, 20 seconds after the step exited, while a hidden child that writes
its own log let it close with the step.

Measured on hosted runs:

| Run | Commit | Tree | Compile step | Build job | Compile requests |
| --- | --- | --- | --- | --- | --- |
| [36631880242](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36631880242) | `bb78abee1` | none (before the cache) | 71 min 53 s | 80 min 16 s | 795 |
| [36645906111](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36645906111) | `84b96e720` | built from scratch, saved the first set | 55 min 52 s | not recorded | not recorded |
| [36739933076](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/36739933076) | `eff5fe381` | restored from `84b96e720`, 23 files changed since | 3 min 4 s | 12 min 44 s | 10 |

The first set was a 6,786,436,758 byte tree in one part of 672,688,710 bytes. The warm run's restore
step took 68 seconds, and its notice read "Build cache: restored the tree built from
84b96e720b8516895bf6b74b0429f46592b3639d (run 633); 23 files changed since." It published `md3-v176` and saved its own tree as the next set (run 634, one
part). One warm run is one measurement: a change to a header that many sources include still
rebuilds all of them, so a later run can take far longer than this one.

## Payload DLLs

`BambuStudio.dll` loads OpenCascade (`TK*.dll`), FFmpeg (`avcodec-61.dll`, `avutil-59.dll`,
`swscale-8.dll`, `swresample-5.dll`), GMP and MPFR, FreeType and `WebView2Loader.dll`, and both it
and the `bambu-studio.exe` launcher use the Visual C++ runtime (`MSVCP140`, `VCRUNTIME140`,
`CONCRT140`). All of them must ship in the payload:

- `bambustudio_copy_dlls` (top-level `CMakeLists.txt`) copies the dependency DLLs from the dependency
  prefix, and `src/CMakeLists.txt` installs the list it returns. It is called for multi-config
  generators (Visual Studio) and single-config ones (Ninja, which the workflow uses) alike. Release
  `md3-v143` was built while only the Visual Studio branch called it: its payload held
  `BambuStudio.dll` alone, `LoadLibrary` failed with error 126 (a dependency not found), and the
  launcher exited with -1 before the app could write a log line.
- `InstallRequiredSystemLibraries` installs the Visual C++ runtime beside the app (app-local
  deployment), so a Windows machine without the VC++ redistributable can start it.
- The step "Verify the payload carries every DLL the app imports" runs
  `scripts/ci/check_payload_imports.py` on `install-dir` before anything is packaged. It reads the
  import and delay-import tables of the launcher and `BambuStudio.dll`, follows every payload DLL they
  pull in, and fails the build when an imported DLL is neither in the payload nor a Windows system DLL.
  The Visual C++ runtime counts as payload, not as Windows. Run it locally on any unpacked payload:
  `py -3 scripts/ci/check_payload_imports.py <payload-dir>`; on the `md3-v143` payload it lists 24
  missing DLLs.

The launcher also records its early decisions in `%TEMP%\bbs-launcher-trace.log` (OpenGL check, Mesa
choice, `BambuStudio.dll` load result), which is where a start that ends before logging can be read.

## CycloneDX payload inventory

`scripts/ci/New-WindowsCycloneDxSbom.ps1` emits `BambuStudioMD3.cdx.json` as CycloneDX 1.6. Every
installed file is represented as a `file` component with a relative path, byte count, and lowercase
SHA-256 digest. The document binds its top-level application component to the Bambu Studio version,
repository, and 40-character source commit.

The generator rejects an empty payload, payload/output path overlap, source reparse points, duplicate
component names, a missing `bambu-studio.exe`, or malformed component digests. The release job
revalidates the document, requires at least 1,000 components, checks the version and commit-bound
source URL, and enforces GitHub's 16 MiB SBOM-attestation limit.

## Squirrel release assets and attestations

After validating the downloaded build artifact, the release job creates build-provenance and SBOM
attestations for `Setup.exe`. A candidate release contains:

- `Setup.exe`;
- `Setup.exe.sha256`;
- `RELEASES`;
- one `*-full.nupkg` and any generated `*-delta.nupkg` files;
- `BambuStudioMD3.cdx.json`.

The bootstrapper is intentionally unsigned and may trigger an unknown-publisher or SmartScreen
warning. Verify download integrity with:

```powershell
Get-FileHash .\Setup.exe -Algorithm SHA256
Get-Content .\Setup.exe.sha256
gh attestation verify Setup.exe --repo Ding-Ding-Projects/BambuStudio
```

The checksum and GitHub attestations are not Authenticode signatures and do not create publisher
identity. No signing certificate, private key, signing service, or signing credential is requested.

## Draft-to-immutable publication

The release job reads the repository immutable-release setting and fails if it is not enabled. It
creates a draft containing the complete Squirrel feed, verifies target, names, sizes, and GitHub
SHA-256 digests against the local candidate, resolves latest status, and publishes the validated
draft. With immutable releases enabled, the resulting published tag and assets cannot be altered
after publication.

If an error occurs while the matching release is still a draft, the job deletes that draft and its
temporary tag. If state cannot be determined, the target differs, or publication may already have
completed, cleanup fails safe by preserving the release for inspection. A retry removes only a
same-commit leftover draft and validates/reuses a same-commit immutable publication; it never mutates
a published immutable release.

## Verification status

After publication and isolated Squirrel installation, the workflow attempts an optional
hidden-desktop GUI capture. `scripts/md3/Capture-HostedReleaseGui.ps1` reads the successful
installation receipt, checks the release tag and source commit, and rehashes the installed
executable against both the installed-file and full-package hashes. It bootstraps a pinned
revision of the headless capture tool into a runner-local Python environment, creates a new
application data directory, and attempts eleven workspace and Preferences surfaces. A task-owned
public RSA key encrypts a ZIP of the original PNGs with a fresh AES-256-GCM key and nonce per run;
RSA-OAEP-SHA256 wraps the AES key. The encryption binds the run ID, source commit, release tag,
installed executable hash, and image hashes as authenticated data. The private RSA key stays
DPAPI-protected in the release operator's local application data, outside this repository.
The workflow uploads only the encrypted ZIP, its small envelope, and `receipt.json` as a 30-day
run artifact. Once a supported hosted capture starts with a new output directory, its receipt
records later preflight and capture failures. Unsupported hosts and existing output directories
are rejected before any files are changed. Raw images stay on the disposable runner; they are
never uploaded or published in plaintext.
This step uses `continue-on-error`, so capture availability is not a release gate.

> [!IMPORTANT]
> AES-GCM checks that the encrypted bundle matches its supplied authenticated metadata.
> Anyone with the public key can encrypt a different bundle, so encryption alone does not
> authenticate its GitHub origin. Before local decryption or promotion, the operator must use
> `gh run view` to compare the exact run ID and source commit, then independently verify the
> published release target and asset hashes against the downloaded release and installation
> receipt. A matching self-reported envelope is not enough.

The receipt records the source commit, release tag, installer and executable hashes, capture
method, rendered-frame hashes, pixel metrics, and status. Its successful state is
`encrypted_capture_pending_restricted_review`. It is provenance and automated pixel evidence,
not reviewed GUI behavior. The operator runs `scripts/md3/Open-HostedReleaseGuiEvidence.ps1`
with the exact run ID, source commit, release tag, and installed executable hash. That helper
uses the local DPAPI key, checks the authenticated binding, enforces a fixed eleven-name image
allowlist and ZIP size limits, and validates every image hash before extraction. The operator
receives all eleven files only after they have been written and rehashed in a unique sibling
directory that is atomically renamed into place. Existing output is never overwritten. The
operator must inspect the decrypted pixels for visual quality and private content before any image is
retained, embedded, or published. The fresh disposable profile does not import the user's local
installation or data. A missing or failed capture must not be described as verified GUI behavior.

Before a candidate is accepted, run the local release contract and one-click checks, build the real
Squirrel output, inspect the README capture matrix from the built artifact, and record the exact
commit, Actions run, release tag, installer SHA-256, Squirrel package names, SBOM component count,
attestation verification, immutable state, and reviewed screenshot set. A pending, cancelled, or absent
remote result is not release proof.

## Verification-only hosted workflow

`.github/workflows/verify-release-evidence.yml` is an independently dispatched verification
workflow. It never compiles the application, creates a release, changes a tag, or gates the
publication workflow. Supply the existing immutable `release_tag`, its exact
`expected_source_commit`, and a `verification_scope`. Start with `diagnostic` to inspect one
English, light, 100%, 1200x800 tuple. Use `behavior` for the six language/theme hosted jobs,
each of which records four requested scales at two viewport sizes. The verification checkout's
commit is recorded separately from the immutable release source commit.
The narrowly scoped `codex/hosted-behavior-verifier` push trigger performs a diagnostic run
against `md3-v125` and `c5df6199e1a83b1c94be12e999c0b322fded8730` while the new verifier
is reviewed on its own branch. It does not trigger for other branches or replace manual matrix
dispatch.

Each job installs and validates the published Squirrel package in a fresh hosted Windows runner
using `Verify-HostedSquirrelInstall.ps1`. It then installs the pinned headless tool and Pillow in
job-local Python, invokes `drive-packaged-behavior.py` against the installed executable, and
encrypts the driver's own report, images, and attributable restricted logs. The separate
publication workflow retains its fixed eleven-surface schema 1 capture. The schema 2 diagnostic
does not launch that older capture route after the behavior driver, so a failed application launch
still preserves its report and restricted logs when present. Only one behavior tuple includes the
complete workflow drive; other tuples inspect localized layout. A diagnostic result never claims
the matrix passed.
The workflow uses a bounded timeout, two concurrent matrix jobs, no cancellation of existing runs,
and a safe failure upload. Raw screenshots, private profiles, and unreviewed behavior reports are
never attached in plaintext.

The version 2 encrypted envelope binds the hosted run, release tag, release source commit,
verification commit, installed executable hash, and the exact manifest of capture images,
behavior reports, behavior images, and driver-owned restricted application logs. The manifest names each tuple and records every byte length
and SHA-256. `Open-HostedReleaseGuiEvidence.ps1` retains schema 1 read support and requires an
explicit expected verification commit for schema 2. It rejects duplicate names, traversal, extra
or missing entries, invalid counts, and hash mismatches before releasing files to a new local
directory. A partial capture can carry encrypted diagnostics, but its receipt remains explicitly
partial and cannot be accepted as verified GUI evidence. Missing printer, camera, or provider
access is reported separately from a failed probe, launch, or teardown. Operators must inspect
decrypted pixels and reports for privacy before retaining or publishing them.
Windows Error Reporting dumps are omitted with an explicit receipt reason because a global
application-name match does not prove the PID, process creation interval, and installed executable
identity of a particular hosted run. A failed fixed-surface capture still encrypts attributable
behavior reports and restricted logs when those files were produced; it keeps a partial verdict.
Schema 2 uses distinct diagnostic, behavior-pending-review, and partial statuses. Eight tuple
reports are required for a complete behavior job; one report is required for a diagnostic job.
The hosted manifest fixture check deliberately supplies traversal and duplicate paths and requires
both to be rejected before private-key access or extraction.
It also encrypts a synthetic failed diagnostic with no image or restricted log, verifies the
partial receipt and exact report inventory, and checks that metadata and authenticated binding
reach the owner-key boundary without extracting plaintext on the hosted runner.

## Owner key initialization and public-key versions

The original `hosted-gui-public.pem` remains recorded as `hosted-gui-public-v1.pem` for historical
envelopes. Its matching local DPAPI slot, when it exists, is the legacy
`BambuStudio\HostedGuiEvidence\private-key.dpapi` file under the current user's local application
data. A missing legacy private key cannot be recreated from the public PEM or an encrypted bundle.
Those historical bundles remain unreadable unless the original protected key is recovered by its
owner. Do not substitute a new key and claim old evidence was reviewed.

After owner review, run `scripts/md3/Initialize-HostedGuiEvidenceKey.ps1 -Initialize` locally under
the account that will review evidence. The script creates a fresh RSA key, protects its private
PKCS#8 bytes with DPAPI CurrentUser in a distinct slot named by the public SPKI SHA-256, and writes
only `hosted-gui-public-v2.pem` to the repository checkout. It refuses an existing public file or
protected slot and cannot run in Actions. Never commit, upload, log, or disclose the protected key
file or its unprotected bytes. Review and commit only the public PEM. The version 2 capture route
fails closed until that public PEM is present in the verifier checkout.

New schema 2 envelopes record `key_id` and `public_key_sha256`, both the same public SPKI SHA-256.
The opener selects only the exact archived version 1 or approved version 2 public key by that ID,
then locates its corresponding local protected slot. Unknown or mismatched IDs are rejected before
private-key access; hosted fixtures cover both cases. The schema 1 reader and its legacy slot remain
supported. A successful envelope validation still requires owner decryption and privacy review
before any report or image is promoted.
Every newly produced envelope uses the approved version 2 public recipient, including schema 1
captures from the independent release publication workflow. Schema number describes the evidence
inventory format, not the encryption key version. Historical envelopes still select the archived
version 1 public key and its legacy local slot by their recorded fingerprint.
