# Delivery package version

Squirrel compares package versions independently of the application version. Packaging accepts `-PreviousPackageVersion` with exactly three nonnegative numeric components supported by `System.Version`. `BAMBU_PREVIOUS_PACKAGE_VERSION` supplies the default. Invalid components or an exhausted patch component stop packaging.

The ordinary candidate remains `major.minor.(patch*1000+ReleaseNumber)`. If that candidate does not exceed the previous package version, packaging increments the previous version's third component. Product `2.8.4`, release `228`, and previous package `2.8.4813` therefore produce `2.8.4814`, rather than the decreasing `2.8.4228`.

The one-click build queries every non-draft release through authenticated `gh api --paginate`, reads matching `BambuStudioMD3-<version>-full.nupkg` asset names, and uses the maximum of the published versions and the explicit floor. This lookup runs only while preparing a package, never inside the installed application or updater. It does not pass credentials as arguments.

For reproducible offline packaging, supply the last published package version explicitly:

```powershell
./scripts/windows/Invoke-OneClickBuild.ps1 -ReleaseNumber 228 -PreviousPackageVersion 2.8.4813
```

If release discovery is unavailable and no explicit floor exists, the one-click package preparation stops rather than assuming no earlier package exists. With an explicit floor, offline preparation continues with a warning that published-version verification was unavailable. A successful lookup with no matching full packages uses the ordinary product/release candidate.

This change was implemented under the requested speed mode. No tests, lint, static analysis, runtime checks, or screenshots were run. Package generation and published update delivery remain separate evidence duties.

## Native invocation and failed-production evidence

The producer quotes every native Squirrel argument with Windows CRT backslash and quote rules. Paths with spaces remain one argument, including the input package, release directory, and setup icon. No short-path alias is required.

Squirrel runs inside its unique task-owned temporary workspace. Standard output and standard error are retained there as `squirrel.stdout.log` and `squirrel.stderr.log`, alongside the input package, generated output, and any tool-created working-directory logs. A failed packaging or tool-preparation operation reports its retained absolute directory and leaves it available for diagnosis. Successful packaging removes its temporary workspace only after output checks, final copies, and checksum creation succeed; the existing resolved-path and temporary-name safety checks still apply. Failed temporary material is not automatically reused or deleted by another invocation.

The native argument and retention repair was not exercised with tests or a runtime invocation in this speed-mode implementation lane. Production packaging must establish its delivery verdict separately.

## Native release source-path metadata

`cmake/modules/ReleaseSourcePaths.cmake` applies MSVC `/experimental:deterministic` and `/pathmap:<physical>=<logical>` to C and C++ compilation. Repository, current child-project source, generated build, and dynamically resolved user-profile roots receive logical source/build prefixes. Diagnostics retain meaningful file names and locations. Executable, shared-library, and module linking use `/PDBALTPATH:%_PDB%`, so the embedded PDB reference retains its basename without a build-host directory. The actual PDB is not relocated. The producer enables this route only for the inspected MSVC 19.51-or-newer baseline; that restriction is a conservative support boundary, not evidence that older MSVC releases lack the options. Both native-backslash and normalized-forward-slash roots are emitted explicitly, including before mixed-separator suffixes. This route does not rewrite compiled bytes.

The root application CMake project and dependency superbuild include the same module. The superbuild passes it through `CMAKE_PROJECT_INCLUDE` to every CMake dependency. OpenSSL's Perl/nmake route receives the same compiler mapping through its scoped `_CL_` environment. No developer identity or host path is checked into the mapping.

The one-click application producer forces configuration when its cache lacks `BAMBU_RELEASE_SOURCE_PATH_POLICY:INTERNAL=msvc-pathmap-v1`. That marker establishes configuration currency only. Existing native outputs, static libraries, and precompiled headers cannot acquire source-path mapping retroactively. Reconfigure affected child projects and rebuild their affected objects before linking the release. Keep read-only cache inputs unchanged and copy affected inputs into a task-owned build when repair is needed.

For the release-228 production findings, rebuild application objects under the new flags, rebuild `dep_OpenMeshCraft` and `dep_libgit2`, and rebuild/relink the affected OCCT targets `TKBRep`, `TKCDF`, `TKernel`, `TKG3d`, `TKService`, `TKTopAlgo`, and `TKV3d` through their owned `dep_OCCT` child project before restaging. Unaffected dependency outputs may remain when their provenance and final payload scan support retention. Changing compiler flags does not update completed ExternalProject stamps by itself; reconfigure the owned child and run its actual build/install route. A full final native/payload inspection must establish that retained inputs did not reintroduce physical paths.

The source repair was not built or exercised in this speed-mode lane. Option availability was inspected in the installed MSVC 19.51.36248 producer; final behavior remains a production-build responsibility. Microsoft documents the PDB basename mechanism in [the linker reference](https://learn.microsoft.com/en-us/cpp/build/reference/pdbaltpath-use-alternate-pdb-path), and LLVM's [MSVC-compatible path mapping implementation](https://github.com/llvm/llvm-project/pull/202830) records the compiler mapping syntax and deterministic prerequisite.
