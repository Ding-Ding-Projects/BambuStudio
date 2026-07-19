# Windows CI and release supply chain

## Trigger and publication policy

`.github/workflows/build_all.yml` is the fork's Windows acceptance and publication workflow. A
code-bearing push or manual dispatch builds the Windows candidate; a pull request targeting
`master` runs the same Windows build without the release job. Every push still triggers the workflow,
but a lightweight Windows path-classification job skips build and release for deleted refs and when
every changed path is Markdown. That allows the final run/checksum documentation handoff without
recursively minting another release.

Every successful code-bearing non-pull-request run is designed to publish one uniquely tagged
release. Tags include the application version and workflow run number. All attempts of a rerun
therefore converge on the same tag instead of creating duplicate releases. A matching published
release is accepted only when it is non-draft and immutable, targets the same commit, has exactly the
expected three assets, and its downloaded checksum, GitHub asset digests, installer archive, and
commit-bound SBOM validate.
Release jobs are serialized. Immediately before first publication, the job resolves the current
`master` tip: only an artifact built from that exact tip may become latest, while superseded master
and non-default-ref builds stay non-latest.

## Windows acceptance gates

The reusable build resolves or rebuilds the dependency cache, then performs these Windows gates:

1. Build and type-check DeviceWeb through the product CMake toolchain's hash-pinned Node/pnpm
   versions and frozen `pnpm-lock.yaml`, fail on a high-severity audit finding, then reject a stale
   generated route tree.
2. Parse PowerShell/JSON/JavaScript inputs; run DeviceWeb, language-resource, Pages language,
   ownership-generator, and SBOM-generator fixture tests.
3. Configure and install the production native Release build with `SLIC3R_BUILD_TESTS=OFF`.
4. Configure a separate test tree with `SLIC3R_BUILD_TESTS=ON`, then build and run only
   `libnest2d_tests` and `language_mode_tests` through CTest.
5. Launch the unsigned native app only on the disposable runner and capture light English, dark
   Cantonese, and light bilingual evidence.
6. Generate a CycloneDX 1.6 inventory from the exact installed payload.
7. Build the NSIS installer and validate its archive with 7-Zip.
8. Execute the fresh-install, upgrade, unknown-path, locked-file recovery, language hand-off,
   uninstall, bootstrap-recovery, exact SBOM-payload, and install/Start-menu reparse-guard matrix.

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
It then creates a draft containing all three assets, verifies their target, names, sizes, and GitHub
SHA-256 digests against the local candidate, resolves latest status, and publishes the validated
draft. With repository immutability enabled, the resulting published tag and assets cannot be altered
after publication. Remote Actions are pinned to reviewed 40-character commit SHAs; comments retain
their human-readable major versions.

If an error occurs while the matching release is still a draft, the job deletes that draft and its
temporary tag. If state cannot be determined, the target differs, or publication may already have
completed, cleanup fails safe by preserving the release for inspection. A retry removes only a
same-commit leftover draft; it validates and reuses a same-commit immutable publication without
comparing nondeterministic rebuilt NSIS/SBOM bytes. It never attempts to mutate a published immutable
release.

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
