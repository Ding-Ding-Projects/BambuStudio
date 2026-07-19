# Windows CI and release supply chain

## Trigger and publication policy

`.github/workflows/build_all.yml` is the fork's Windows acceptance and publication workflow. A
commit-bearing push or manual dispatch builds the Windows candidate; a pull request targeting
`master` runs the same Windows build without the release job. Branch/tag deletion pushes are ignored
because they have no source tree to package.

Every successful non-pull-request run is designed to publish one uniquely tagged release. Tags
include the application version, workflow run number, and attempt, so an existing tag is never
overwritten. Release jobs are serialized. Immediately before publication, the job resolves the
current `master` tip: only an artifact built from that exact tip may become latest, while superseded
master and non-default-ref builds stay non-latest.

## Windows acceptance gates

The reusable build resolves or rebuilds the dependency cache, then performs these Windows gates:

1. Build and type-check DeviceWeb with the locked npm dependency tree.
2. Parse PowerShell/JSON/JavaScript inputs; run DeviceWeb, language-resource, Pages language,
   ownership-generator, and SBOM-generator fixture tests.
3. Configure and install the native Release build with `SLIC3R_BUILD_TESTS=ON`.
4. Build and run `libnest2d_tests` and `language_mode_tests` through CTest.
5. Launch the unsigned native app only on the disposable runner and capture light English, dark
   Cantonese, and light bilingual evidence.
6. Generate a CycloneDX 1.6 inventory from the exact installed payload.
7. Build the NSIS installer and validate its archive with 7-Zip.
8. Execute the fresh-install, upgrade, unknown-path, locked-file recovery, language hand-off,
   uninstall, and destination-junction behavior matrix.

Failure in any dependency, build, test, capture, SBOM, installer, or archive gate prevents the release
job from starting. The native capture and installer scripts additionally refuse to execute unless
they receive an explicit CI switch on a GitHub-hosted Actions runner.

## CycloneDX payload inventory

`scripts/ci/New-WindowsCycloneDxSbom.ps1` emits `BambuStudioMD3.cdx.json` as CycloneDX 1.6. Every
installed file is represented as a `file` component with a relative path, byte count, and lowercase
SHA-256 digest. The document binds its top-level application component to the Bambu Studio version,
repository, and 40-character source commit.

The generator rejects an empty payload, payload/output path overlap, source reparse points, duplicate
component names, a missing `bambu-studio.exe`, or a malformed component digest. The release job
revalidates the document, requires at least 1,000 components, checks the version and commit-bound
source URL, and enforces GitHub's 16 MiB SBOM-attestation limit.

This is a precise installed-file inventory, not a complete dependency-license or vulnerability
analysis. It helps answer which bytes were packaged; it does not by itself certify that every bundled
component is vulnerability-free.

## Attestations and release assets

After validating the downloaded build artifact, the release job creates two GitHub attestations for
the installer with `actions/attest@v4`: build provenance and an SBOM attestation using the generated
CycloneDX document. A candidate published by this workflow must contain exactly:

- `BambuStudioMD3-Setup.exe`
- `BambuStudioMD3-Setup.exe.sha256`
- `BambuStudioMD3.cdx.json`

After downloading the installer, provenance can be checked with:

```powershell
gh attestation verify BambuStudioMD3-Setup.exe --repo codingmachineedge/BambuStudio
```

The SHA-256 sidecar verifies download integrity, while the GitHub attestations bind the installer
digest to the repository's Actions identity. Neither mechanism is a Windows Authenticode signature or
a substitute for trusted publisher identity.

## Draft-to-immutable publication

The release job first reads the repository immutable-release setting and fails if it is not enabled.
It then creates a draft containing all three assets, verifies the draft target commit and exact asset
names, resolves latest status, and publishes the validated draft. With repository immutability
enabled, the resulting published tag and assets cannot be altered after publication.

If an error occurs while the matching release is still a draft, the job deletes that draft and its
temporary tag. If state cannot be determined, the target differs, or publication may already have
completed, cleanup fails safe by preserving the release for inspection. It never attempts to mutate a
published immutable release.

Enabling repository immutable releases is a separate administrator action; the workflow check proves
that it was enabled at publication time. Documentation should not treat the candidate release as
immutable until the final workflow has passed and its published state has been inspected.

## Permissions and external publishing

The workflow default is read-only repository access. Only the release job receives `contents: write`,
`id-token: write`, `attestations: write`, and `artifact-metadata: write`; checkout does not persist
credentials. Upstream WinGet and Homebrew publication remains gated to the upstream Bambu Lab
repository, so this fork cannot update those feeds automatically.

No Postman collection is applicable because this release surface exposes no HTTP API.

## Verification status

The last fully published baseline before these candidate gates is commit `1f1ecb960`, Actions run
`29671557311`, and release `md3-windows-v02.08.01.55-r6.1`. The expanded tests, CycloneDX asset,
attestations, visual evidence, and draft-to-immutable path still require a successful candidate run.
Final evidence should record the candidate commit, Actions run URL/ID, release tag, installer SHA-256,
SBOM component count, attestation verification, immutable state, and reviewed capture artifact.
