# Native Windows installer and release pipeline

## Behavior

Every push to `master` and every manual dispatch runs the Windows dependency/build workflow. After a
successful native build, NSIS packages the installed C++ payload as `BambuStudioMD3-Setup.exe`. The
release job validates the NSIS archive, emits a SHA-256 checksum, creates a unique immutable-style tag,
and publishes a non-draft GitHub Release marked as latest.

Pull requests run the same Windows build but do not publish a release. Dependency-only manual runs
also skip release publication.

## Configuration

- Workflow: `.github/workflows/build_all.yml`
- Reusable native build: `.github/workflows/build_bambu.yml`
- Installer definition: `packaging/windows/BambuStudioMD3.nsi`
- Install directory: `%LOCALAPPDATA%\Programs\Bambu Studio MD3`
- Stable latest-download asset: `BambuStudioMD3-Setup.exe`

Release tags include the application version, workflow run number, and run attempt, so an existing tag
or immutable release is never overwritten.

## Failure modes

- A native build, missing dependency cache, installer compile error, or archive validation error stops
  the workflow before release creation.
- The release job requires exactly one installer artifact; zero or duplicate matches fail closed.
- Release publication requires the workflow's scoped `contents: write` token.
- The installer is unsigned until a code-signing identity is configured. The checksum provides
  integrity verification but not publisher identity.

## Security considerations

The installer requests user-level privileges, uses a fixed per-user target, and guards recursive
uninstall. Release checksums are generated from the exact artifact uploaded by the successful build.
Fork releases do not automatically invoke the upstream WinGet or Homebrew publishing integrations.

## Verification

The NSIS definition was compiled locally against the portable artifact from Windows CI run
`29665576610`. 7-Zip identified a Unicode NSIS 3 archive, tested all 6,743 files, and reported no
errors. The resulting setup executable reports version `02.08.01.55` and remains intentionally
unsigned.
