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
GitHub asset digests before publishing the draft. The build job does not create a cache prerelease or
any other secondary GitHub Release.

The release job resolves the current default-branch tip immediately before publication. Only an
artifact built from that exact tip may become latest; superseded or non-default-ref builds remain
non-latest.

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
