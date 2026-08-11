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
Chuts rather than Actions test jobs. The committed local checks remain available and are run before a
manual release or before accepting a candidate build. A workflow build still fails on compiler,
dependency, SBOM, or Squirrel packaging failures.

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

Before a candidate is accepted, run the local release contract and one-click checks, build the real
Squirrel output, inspect the README capture matrix from the built artifact, and record the exact
commit, Actions run, release tag, installer SHA-256, Squirrel package names, SBOM component count,
attestation verification, immutable state, and reviewed HuiShot set. A pending, cancelled, or absent
remote result is not release proof.
