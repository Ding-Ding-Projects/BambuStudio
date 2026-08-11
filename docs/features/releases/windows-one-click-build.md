# One-click Windows build and installer

`OneClickBuildInstaller.cmd` is the supported local entry point for compiling a Release payload
and packaging it with Squirrel.Windows. Double-click it in File Explorer or run it from a terminal:

```powershell
.\OneClickBuildInstaller.cmd
```

The launcher delegates to `scripts/windows/Invoke-OneClickBuild.ps1`, writes a transcript to
`artifacts/windows/one-click-build.log`, and pauses at the end when launched interactively. Build
outputs are written to `artifacts/windows/`:

- `squirrel/Setup.exe` — unsigned Squirrel bootstrapper;
- `squirrel/RELEASES` — update-feed index;
- `squirrel/BambuStudioMD3-<version>-full.nupkg` and any generated delta packages;
- `squirrel/Setup.exe.sha256` — SHA-256 sidecar for the bootstrapper;
- `BambuStudioMD3.cdx.json` — CycloneDX SBOM bound to the source commit.

## What it does

The workflow checks for at least 40 GB of free space and installs missing ordinary prerequisites:
Git and Git LFS, Visual Studio 2022 C++ Build Tools, a complete Windows SDK, CMake, Strawberry
Perl, and 7-Zip. Strawberry Perl supplies the Windows `pkg-config.bat` fallback when the native
`pkgconfiglite` executable is not present; the build exports that wrapper explicitly so CMake does
not mistake Strawberry's extensionless helper script for a runnable executable. Existing supported
installations are reused. Tool installation uses `winget` silently with package/source agreement
acceptance; the shared toolchain helper retains its publisher and pinned-hash checks for vendor
fallbacks. The dependency superbuild supplies the product's hash-pinned Node.js and pnpm versions,
so the workflow does not replace an unrelated system Node installation.

Squirrel.Windows 2.0.1 is fetched only when it is not already available in the user NuGet cache.
The package is downloaded from NuGet, checked against the committed SHA-256 pin, extracted into a
user-local cache, and then used by `scripts/windows/Invoke-SquirrelPackage.ps1`. No signing command,
certificate, or signing credential is accepted.

The script fetches Git LFS objects, compiles dependencies, compiles the Release application, stages
the CMake install payload, downloads and verifies the same hash-pinned Mesa llvmpipe fallback used
by CI, creates the CycloneDX SBOM, creates the Squirrel NuGet package with the exact source commit
and repository metadata, runs `Squirrel.exe --releasify`, validates `Setup.exe`, `RELEASES`, the
full package, and unsigned Authenticode status, then writes the checksum sidecar.

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

Automation can set `BAMBU_ONE_CLICK_NO_PAUSE=1` before calling the CMD launcher. Only one copy may
run at a time; a cross-process mutex rejects a second launch before it can write to the shared build
caches.

## Failure modes and recovery

- Dependency installation can require Windows elevation or a restart. Rerun the same command after
  approving the vendor installer or restarting; completed prerequisites are detected and reused.
- A clean build can require more than 40 GB and several hours. The transcript identifies the exact
  failed phase and exit code.
- Network access is required for missing packages, Git LFS objects, the pinned Mesa archive, and the
  hash-pinned Squirrel.Windows NuGet package when it is not cached.
- Tracked working-tree edits can be compiled locally, but the Squirrel nuspec can record only the
  current Git commit. The workflow warns when this makes the local payload non-reproducible.
- The generated Squirrel package must contain `lib/net45/bambu-studio.exe`; a missing executable,
  missing `RELEASES`, mismatched checksum, or signed Setup.exe fails closed.

No pattern, source content, or build log is transmitted except to the declared package, Git/LFS,
and pinned artifact endpoints needed by the build.
