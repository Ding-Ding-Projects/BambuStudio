# Native Windows installer and release pipeline

## Behavior

Every commit-bearing push and every manual dispatch runs the Windows dependency/build workflow.
Branch/tag deletion events are ignored because they have no source tree to package. After a
successful native build, NSIS packages the installed C++ payload as `BambuStudioMD3-Setup.exe`. The
release job validates the NSIS archive, emits a SHA-256 checksum, creates a unique tag, and publishes
exactly one non-draft GitHub Release. A release is marked latest only when its commit is still the
current `master` tip; superseded or non-default refs remain non-latest.

Pull requests run the same Windows build but do not publish a release. The public workflow has no
dependency-only success path, so every successful push or manual run reaches release publication.

## Configuration

- Workflow: `.github/workflows/build_all.yml`
- Reusable native build: `.github/workflows/build_bambu.yml`
- Installer definition: `packaging/windows/BambuStudioMD3.nsi`
- Owned-file list generator: `packaging/windows/GenerateUninstallInclude.ps1`
- Install directory: `%LOCALAPPDATA%\Programs\Bambu Studio MD3`
- Stable latest-download asset: `BambuStudioMD3-Setup.exe`

Release tags include the application version, workflow run number, and run attempt, so an existing tag
or immutable release is never overwritten.

## Failure modes

- A native build, missing dependency cache, installer compile error, or archive validation error stops
  the workflow before release creation. A cache miss rebuilds dependencies; only a failed dependency
  build stops publication.
- The release job requires exactly one installer artifact; zero or duplicate matches fail closed.
- Release publication requires the workflow's scoped `contents: write` token.
- The installer is unsigned until a code-signing identity is configured. The checksum provides
  integrity verification but not publisher identity.

## Security considerations

The installer requests user-level privileges and uses a fixed per-user target. At build time, the
payload is converted into explicit per-file `Delete` and deepest-directory-first `RMDir` commands.
Uninstall therefore removes product-owned paths only, never uses recursive directory deletion, and
leaves unknown paths and their non-empty parent directories intact. An ownership marker prevents
installation over an unrelated non-empty directory. Upgrades run the prior ownership-scoped
uninstaller first so obsolete product files do not survive. Source or live-destination reparse points
and unsafe paths fail closed. Locked files preserve the uninstall registration and allow a retry.
Release checksums are generated from the exact artifact uploaded by the successful build. Fork
releases do not automatically invoke the upstream WinGet or Homebrew publishing integrations.

## Verification

The NSIS definition was compiled locally against the portable artifact from Windows CI run
`29665576610`. The ownership generator emitted 6,740 file-delete commands and 276 non-recursive
directory-removal commands, with no `RMDir /r`. 7-Zip identified a Unicode NSIS 3 archive, tested all
6,743 files, and reported no errors. The resulting setup executable reports version `02.08.01.55`
and remains intentionally unsigned.
