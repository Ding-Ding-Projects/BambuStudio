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
- `squirrel/BambuStudioMD3-<version>-full.nupkg` and any generated delta packages. `<version>` is
  `<major>.<minor>.<patch*1000+N>`, for example `2.8.2106` for product 2.8.2 with `N` = 106
  (Squirrel.Windows accepts only three numeric parts and compares prerelease labels as strings),
  so packages rank correctly even when `version.inc` is unchanged. A one-click build resolves `N`
  from `-ReleaseNumber`, then `BAMBU_RELEASE_NUMBER`, then the latest `md3-v<N>` release seen by
  `gh` plus one; with none of those the package version falls back to the product version alone
  (`2.8.2-build61`) and the log says so. Hosted release builds use the run number of the Windows
  build and release workflow instead: a release number read before the release job assigns the
  tag repeats when builds queue behind one another (`md3-v155` and `md3-v156` both carry
  `2.8.4155`), and the run number only grows in push order;
- `squirrel/Setup.exe.sha256` — SHA-256 sidecar for the bootstrapper;
- `BambuStudioMD3.cdx.json` — CycloneDX SBOM bound to the source commit.

## What it does

The workflow checks for at least 40 GB of free space and installs missing ordinary prerequisites:
Git, Visual Studio 2022 or 2026 C++ Build Tools, a complete Windows SDK, CMake, Strawberry
Perl, and 7-Zip. Strawberry Perl supplies the Windows `pkg-config.bat` fallback when the native
`pkgconfiglite` executable is not present; the build exports that wrapper explicitly so CMake does
not mistake Strawberry's extensionless helper script for a runnable executable. Existing supported
installations are reused. Stale Visual Studio registrations whose required files or default x64 compiler are absent are skipped, and product identity comes from the same usable instance as its path. If none is usable (or the SDK is missing), the Microsoft-signed VS 2026 Stable bootstrapper receives the explicit Bambu-owned `%LOCALAPPDATA%\BambuStudioMD3\toolchain\BuildTools2026` installation path and `--norestart`. It does not remove or repair unrelated registrations. Administrator approval can still be required; the bootstrap does not bypass it. Other tool installation uses `winget` silently with package/source agreement
acceptance; the shared toolchain helper retains its publisher and pinned-hash checks for vendor
fallbacks. The dependency superbuild supplies the product's hash-pinned Node.js and pnpm versions,
so the workflow does not replace an unrelated system Node installation.

Squirrel.Windows 2.0.1 is fetched only when it is not already available in the user NuGet cache.
The package is downloaded from NuGet, checked against the committed SHA-256 pin, extracted into a
user-local cache, and then used by `scripts/windows/Invoke-SquirrelPackage.ps1`. No signing command,
certificate, or signing credential is accepted.

The script compiles dependencies, compiles the Release application, stages
the CMake install payload, downloads and verifies the same hash-pinned Mesa llvmpipe fallback used
by CI, creates the CycloneDX SBOM, creates the Squirrel NuGet package with the exact source commit
and repository metadata, runs `Squirrel.exe --releasify`, validates `Setup.exe`, `RELEASES`, the
full package, and an empty PE security directory on Setup.exe (unsigned), then writes the checksum sidecar.

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
- Network access is required for missing packages, the pinned Mesa archive, and the
  hash-pinned Squirrel.Windows NuGet package when it is not cached.
- Tracked working-tree edits can be compiled locally, but the Squirrel nuspec can record only the
  current Git commit. The workflow warns when this makes the local payload non-reproducible.
- The generated Squirrel package must contain `lib/net45/bambu-studio.exe`; a missing executable,
  missing `RELEASES`, mismatched checksum, or a non-empty PE security directory on Setup.exe fails closed.

No pattern, source content, or build log is transmitted except to the declared package, Git,
and pinned artifact endpoints needed by the build.

Visual Studio instance selection follows [Microsoft's command-line installation parameters](https://learn.microsoft.com/en-us/visualstudio/install/use-command-line-parameters-to-install-visual-studio). The helper regression exercises stale and usable registrations, product/path agreement, a missing default compiler, and the explicit quoted installation target without running an installer. This repository tracks no Git LFS objects, so the supported script does not install or invoke Git LFS.

Microsoft requires each side-by-side installation to have a unique major version, edition, and update channel. An existing registration with the same combination can still block the explicit target; report that installer error without deleting or rewriting the unrelated registration. Fixture success does not prove a host installation or native build.

Usable VS 2022 installations keep the existing CMake 3.21 minimum. VS 2026 requires CMake 4.2 or newer, below 5.0, for the documented `Visual Studio 18 2026` generator. Both the dependency and application configurations pin `CMAKE_GENERATOR_INSTANCE` to the selected path; VS 2026 also supplies the numeric four-part `installationVersion` from that same record using CMake's documented `location,version=` syntax. The from-source route passes the selected major and product to `build_win.bat`, with `BAMBU_VS_INSTALLATION_PATH` validated against registration, required files, supported major, and unsafe CMD characters before expansion. For VS 2026, `BAMBU_VS_INSTALLATION_VERSION` must also match that exact registration. Without that override, the batch entry point retains its existing detection. The VS 2026 Stable fallback uses a different major/channel from an incomplete VS 2022 Release registration; native build compatibility still requires a real build result.

Discovery includes prerelease and incomplete registrations, then validates the required compiler files independently. It also starts the exact x64 MSBuild executable with `-nologo -version` under a 15-second deadline. A startup error, timeout, or missing numeric version rejects that candidate with a diagnostic and continues to the next one; when none is usable, the supported Stable bootstrap runs. It never copies assemblies into or repairs an unrelated installation. Logs identify the selected path, version, prerelease state, and registration completeness; incomplete registration alone does not prove the files are absent. Run `scripts/ci/Test-BuildFromSourceHelpers.ps1 -ProbeHostCMake` for an optional, bounded configure-only fixture using the selected instance's bundled CMake. A configure error remains a toolchain blocker even when direct compiler checks pass. The bootstrap nickname is `BambuMD3`, within Microsoft's ten-character limit.

## Fresh Windows bootstrap and administrator handoff

Both `build.bat /s` and `build-installer.bat /s` execute the shared PowerShell producer and propagate its exit code. The shared root-entry-point launcher now requests administrator approval through the normal Windows `RunAs` consent prompt before acquiring its build mutex, opening its transcript, or installing prerequisites. The elevated helper console is hidden; the native UAC consent prompt remains interactive. Already elevated invocations continue directly. A canceled or unavailable elevation returns `1223`. The handoff preserves any existing process-scoped execution policy without changing persistent policy. The complete original argument array is serialized as data, and the elevated host invokes the exact original `build.bat`, `build-installer.bat`, or `OneClickBuildInstaller.cmd` with that array; `-Plan` remains read-only and does not request elevation. Silent mode suppresses launcher chatter, not administrator consent.

The existing bootstrap installs Git, Visual Studio 2026 C++ Build Tools and its Windows SDK when missing, CMake, Node, Strawberry Perl, Python 3, and 7-Zip. If WinGet is absent, it now uses Microsoft's `Microsoft.WinGet.Client` bootstrap and `Repair-WinGetPackageManager -AllUsers`, then refreshes the current process path. This does not change persistent execution policy, package-repository trust, antivirus configuration, or host power state. Both entry points additionally install the stable .NET 10 SDK needed for their automation companion, which is staged before the build-only route returns.

Microsoft documents the WinGet bootstrap in [Windows Package Manager installation](https://learn.microsoft.com/en-us/windows/package-manager/winget/). Network, publisher, package-service, administrator-consent, and machine-registration blockers are reported rather than treated as successful installation. The native toolchain installer retains its existing `--norestart` behavior. Neither entry point restarts or shuts down the host.

For installer packaging, provide `-PreviousPackageVersion` (or `BAMBU_PREVIOUS_PACKAGE_VERSION`) when authenticated release discovery is unavailable. Bootstrap does not invent release history or install credentials. A valid dependency cache may be reused, while stale affected native inputs still require the documented source-path producer repair and actual rebuilding. Both entry points remain production commands; this source repair has not been executed on a fresh Windows installation and makes no fresh-install verification claim.

The original launcher waits for the elevated producer and returns its actual process exit code. A producer `exit 1` terminates that child host directly; the helper's success/failure check applies only when the producer returns normally. Launch exceptions return `1223` separately. These are source-level control-flow facts; cancellation, child failure and fresh-host execution were not exercised in this implementation lane.

The elevation handoff lives in `scripts/windows/Invoke-BuildEntryPoint.ps1`. It accepts only the three exact repository launchers, serializes entry-point identity and the complete original argument array as JSON data, and relaunches the same batch file through PowerShell's native invocation with an argument array. It does not build a raw `cmd /c` command string. Elevated re-entry then calls the producer once. Root `build.bat` retains build-only behavior, while both installer launchers retain packaging behavior. `/s` and `--silent` survive elevation and are filtered only at the producer boundary. Direct producer invocation is an internal route and does not prove execution of either root batch entry point.

At the script boundary, the launcher validates the supported named parameters and binds them through a hashtable rather than positional array splatting. It accepts value parameters `BuildMode`, `OutputDirectory`, `DependencyCacheDirectory`, `ReleaseNumber`, and `PreviousPackageVersion`, plus switches `Install`, `BootstrapOnly`, `Plan`, and `BuildOnly`. Unknown or duplicate names are rejected. Root `build.bat` forces `BuildOnly=true`, including when a caller explicitly supplied false. The original native argument array still re-enters the exact batch file during elevation; script binding happens after that provenance boundary.

The elevation launcher reads only the existing `PSExecutionPolicyPreference` process environment value, validates it against recognized execution-policy values, and preserves it for the child. Absent or `Undefined` values omit the argument. Unknown values stop the handoff. It does not load the optional `Microsoft.PowerShell.Security` module or alter machine/user policy; exact batch entry-point provenance remains unchanged.

CMake bootstrap first retains an existing PATH executable that satisfies the supported minimum and exclusive maximum. Otherwise it checks the selected usable Visual Studio instance's `Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe` with the same version bounds, prepends that directory to the producer's process PATH, and preserves it during PATH refresh. Package-manager/vendor installation remains the fallback when neither candidate qualifies. Discovery happens after elevation, so it does not depend on a caller-only PATH edit surviving `RunAs`. The source gap was confirmed; the purpose of a contemporaneous live WinGet operation was not established and is not attributed to this gap. This repair has no execution proof yet.

## Relocatable OpenCV data lookup

The packaged OpenCV producer enables `OPENCV_RELOCATABLE_DATA_LOOKUP`. Its source patch omits the absolute `OPENCV_INSTALL_PREFIX` macro and emits an empty `OPENCV_BUILD_DIR`; runtime code explicitly skips the two build-directory search branches when that root is empty. This excludes development-tree fallbacks from a relocatable installed package rather than replacing runtime filesystem locations with fictional logical paths.

Explicit data configuration and search-path overrides remain unchanged. The existing `OPENCV_INSTALL_DATA_DIR_RELATIVE` lookup still derives installed data locations from the running module, including its configured relative `etc` relationship. Required/not-found behavior, source-tree discovery from the actual current directory, filesystem checks and diagnostics remain available. The ordinary OpenCV build retains its prior constants when the producer option is disabled.

Reapply the fourth source patch in an owned OpenCV source directory, reconfigure the child with the option enabled, rebuild its generated data header/static library, then relink and restage the application through the exact root batch entry points. Existing `version_string.inc` mapping is separate and remains in place. No tests, builds, runtime interactions or screenshots ran in this repair lane; final payload inspection remains required.

The elevation launcher waits for the exact elevated entry-point host with `Process.WaitForExit`, after acquiring its process handle. It reads the exit code before disposing the handle. It does not use `Start-Process -Wait`, whose descendant-job wait can include idle reusable MSBuild servers after the producer has finished. No descendant is stopped or terminated by this change, and the original batch arguments, hidden host and normal UAC consent remain unchanged. Process completion is still separate from packaging/publication verification.

## Relocatable wxWidgets install prefix

The dependency producer enables `wxBUILD_RELOCATABLE_INSTALL_PREFIX` and applies a source patch to wxWidgets' CMake setup generator and `wxGetInstallPrefix`. For Windows packages, the generated setup header omits the build-machine `wxINSTALL_PREFIX` and enables the relocatable runtime fallback. `WXPREFIX` remains the first explicit override. Otherwise, the fallback obtains the actual executable path from `wxStandardPaths` and returns its containing directory; `wxGetDataDir` retains its existing `share/wx` suffix. This uses a real runtime location rather than a fabricated logical filesystem prefix. Other platforms or builds with the option disabled keep the upstream compile-time prefix behavior.

Reapply the patch in the owned wxWidgets source, reconfigure its setup header, rebuild the affected base object/static library and dependent application output, then run the exact root batch production commands and inspect the final payload. The repair does not modify compiled bytes or loosen publication inspection. No tests, builds, runtime checks or screenshots ran in this source lane.
