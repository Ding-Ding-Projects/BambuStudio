# One-click Windows build and installer

`OneClickBuildInstaller.cmd` is the supported local entry point for compiling a Windows Release
installer from an existing checkout. Double-click it in File Explorer or run it from a terminal:

```powershell
.\OneClickBuildInstaller.cmd
```

The launcher delegates to `scripts/windows/Invoke-OneClickBuild.ps1`, writes a transcript to
`artifacts/windows/one-click-build.log`, and pauses at the end when launched interactively. Build
outputs are written to `artifacts/windows/`:

- `BambuStudioMD3-Setup.exe`
- `BambuStudioMD3-Setup.exe.sha256`
- `BambuStudioMD3.cdx.json`

## What it does

The workflow checks for at least 40 GB of free space and installs missing ordinary prerequisites:
Git and Git LFS, Visual Studio 2022 C++ Build Tools, a complete Windows SDK, CMake, NSIS, and 7-Zip.
Existing supported installations are reused. Tool installation uses `winget` silently with
package/source agreement acceptance; the shared toolchain helper retains its publisher and
pinned-hash checks for vendor fallbacks. The dependency superbuild supplies the product's
hash-pinned Node.js and pnpm versions, so the workflow does not replace an unrelated system Node
installation. If winget has a stale NSIS registration, the workflow uses `winget download` to
verify the official NSIS 3.12 package against the package-manifest hash, then extracts a user-local
portable compiler with 7-Zip instead of requiring elevation.

It then fetches Git LFS objects, compiles dependencies, compiles the Release application, stages
the CMake install payload, downloads and verifies the same hash-pinned Mesa llvmpipe fallback used
by CI, creates the CycloneDX SBOM and uninstall manifest, compiles the UTF-8 NSIS installer, checks
the installer archive with 7-Zip, and writes its SHA-256 sidecar.

The default is incremental. Use a clean rebuild when caches may be stale:

```powershell
.\OneClickBuildInstaller.cmd -BuildMode Clean
```

Bootstrap or inspect without compiling:

```powershell
.\OneClickBuildInstaller.cmd -BootstrapOnly
.\OneClickBuildInstaller.cmd -Plan
```

The installer is unsigned. It is not launched automatically. To run it after successful packaging,
make that state-changing choice explicit:

```powershell
.\OneClickBuildInstaller.cmd -Install
```

Automation can set `BAMBU_ONE_CLICK_NO_PAUSE=1` before calling the CMD launcher.
Only one copy may run at a time; a cross-process mutex rejects a second launch before it can write
to the shared build caches.

## Failure modes and recovery

- Dependency installation can require Windows elevation or a restart. Rerun the same command after
  approving the vendor installer or restarting; completed prerequisites are detected and reused.
- A clean build can require more than 40 GB and several hours. The transcript identifies the exact
  failed phase and exit code.
- Network access is required for missing packages, Git LFS objects, and the pinned Mesa archive.
- Tracked working-tree edits can be compiled locally, but the installer can record only the current
  Git commit. The workflow warns when this makes the local payload non-reproducible.
- The source-build choice inside the resulting installer clones the HTTPS `origin`; an SSH-only
  origin is therefore rejected during packaging.

No pattern, source content, or build log is transmitted except to the declared package, Git/LFS,
and pinned artifact endpoints needed by the build.
