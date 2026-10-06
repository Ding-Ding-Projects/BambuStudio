# Isolated delivery builds

The supported commands remain `build.bat /s` and `build-installer.bat /s`.
Repository commands stream native stdout through `Out-Host` so the PowerShell
transcript records their diagnostics. Native nonzero exit codes and terminating
PowerShell exceptions retain their existing failure behavior; stderr is not
suppressed or redirected. Four echo/exit/exception fixture assertions reproduce
the old missing transcript text and verify the repaired output route. Missing
historical output does not establish a production failure's underlying cause.
OpenCV applies its four source patches independently through
`cmake/modules/ApplyPatchesIdempotently.cmake`. A forward check permits an apply;
a reverse check proves a patch already applied. Neither state stops with both
original Git diagnostics. Partial sequences can be retried without rewriting
patches or resetting their source. Nine Git/CMake fixture assertions demonstrate
the old batch rerun failure, partial completion, stable repeat, and conflict
refusal. These checks do not change downloaded production source.
Squirrel tooling is reused only after its retained NuGet archive matches the
pinned SHA-256 and every tool-file byte matches that archive. Legacy NuGet and
old tool caches remain untouched. Verified tools use a content-addressed owned
cache; invalid owned caches are preserved under a unique previous-cache path
before atomic directory promotion. An invalid cache without its ownership marker
stops preparation and remains intact. Extraction uses cache-parent staging so
temporary storage may live on another volume. Twelve fixture-archive assertions cover warm
reuse, tampering, missing, extra, and hidden files/directories, repair, and uncertain ownership without
executing any tool.
`RELEASES` must describe exactly the produced packages with their actual SHA-1
and byte length. Duplicate, unsafe, malformed, and unindexed packages stop
promotion. The new set is validated in sibling staging, promoted by directory
rename, and checked again. Prior output directories remain under unique previous
names; a post-promotion validation failure restores the prior set and retains the
failed candidate. Eleven fixture assertions exercise these cases using a mocked
PE reader; no installer or application is executed.
Production pins `HEAD` before compilation and requires clean tracked source
and no nonignored untracked files, since resources and automation sources can
otherwise enter production without belonging to that commit.
The pinned commit is checked again after compilation and before and after
packaging. A moved commit or changed tracked source stops production instead of
labelling the payload with a later commit. `Test-BuildSourceIdentity.ps1` covers
eleven focused assertions with mocked Git, including the final build-only
source assertion after payload staging; it does not compile or package.
Both entrypoints retain the producer's exact nonzero exit result and forward
named options, including output paths containing spaces. The first route forces
build-only behavior; the installer route includes packaging by default.

`BAMBU_BUILD_JOBS` accepts a positive integer and controls the dependency worker
budget. Invalid values fail instead of silently selecting a default. The root
dependency configure forwards `-DNPROC=<jobs>`, runs one external dependency
project at a time, and generates numbered MSBuild `/m:<jobs>` arguments. During
that build only, a trailing compiler `_CL_` option `/MP1` prevents dependencies
such as OCCT from multiplying MSBuild workers into full CPU-sized compiler pools.
Existing compiler options are preserved, the cap precedes any `/link` boundary,
and the original process environment is restored on success or failure. No
machine-wide setting is changed. This bounds MSBuild and MSVC compiler pools;
it does not promise a limit on unrelated programs or every tool's helper threads.
Direct dependency configuration still accepts explicit `NPROC`, then a nonempty
`CMAKE_BUILD_PARALLEL_LEVEL`, then detected processors. Non-MSVC generated build
arguments retain their `-j<jobs>` form. `Test-DependencyParallelism.ps1` executes
twenty-one focused assertions using script-mode CMake and a stub build, including
the real OpenSSL include-order worker detection, with no
native compilation or package production.

Strawberry detection requires its paired `perl/bin/pkg-config.bat` and `perl.exe`,
independently of other pkg-config installations. The dependency build selects
that Perl interpreter first and checks `Locale::Maketext::Simple` before compiling
OpenSSL. A native pkg-config executable may still serve CMake independently.
Missing tools get an initial winget install and at most one non-destructive
force-install retry. Failed probes never trigger package removal. After both
attempts, a missing tool stops with both exit codes and preserves existing tools.

Focused script checks are `scripts/ci/Test-BootstrapRecovery.ps1` (nine behavioral
assertions) and `scripts/ci/Test-BuildEntryPoint.ps1` (six assertions using a stub
producer). They install nothing and do not compile, package, launch, or execute
the application. These checks do not prove a fresh-machine build or installer.

Set `BAMBU_DEPENDENCY_CACHE` to an existing dependency destination containing
`usr/local/include` and `usr/local/lib` to reuse it without modifying it.
Application build and install output remains inside the current checkout.
Application configuration reuse compares exact install/dependency prefix values
and a recorded content identity. That identity covers relevant committed source
trees, every file in the selected dependency `usr/local` prefix, compiler and
CMake bytes and paths, compiler/Visual Studio versions, generator instance, SDK
selection, and source/install roots. Missing or changed identity causes a fresh
configure. Before reconfiguration, the producer verifies the exact owned build
directory and the cache's source-directory receipt, then preserves only
`CMakeCache.txt` under a unique `artifacts/windows/application-configurations/`
history entry. CMake regenerates discovery without retaining the old dependency
prefix, while object directories remain in place. An unrelated source receipt or
reparse-point target stops this preservation. Content hashing adds a read of the selected dependency files at build
startup; timestamps alone cannot prove their content. Fourteen fixture/stub assertions
verify mismatched-prefix rejection and invalidation for source, dependency,
compiler, and SDK changes, plus application compiler-cap restoration. Seven real
configure-only assertions demonstrate libnoise discovery moving from prefix A to
prefix B and prior-cache/object preservation, with no compiler or native build.
The application production commands also use the process-only `/MP1` suffix,
so root MSBuild concurrency is not multiplied by the application's bare `/MP`.
An incompatible or incomplete cache causes configuration or compilation to stop;
the caller must select a compatible cache or omit the variable for a local build.
FFmpeg package flags quote their include and library paths so an isolated checkout
under a directory containing spaces remains supported. Existing writable local
caches receive the same normalization; external cache destinations stay read-only.

Node.js LTS is installed from the fixed official portable archive, verified against
the publisher SHA-256, into the user build-tools directory. This preserves an
existing global Node version and avoids Windows Installer contention.

The main release workflow builds branch candidates. Only pushes to `main`
automatically publish. Manual dispatch builds by default; publication requires
`publish_release: true` and a `main` ref. Automation test workflows are not a
release dependency. This delivery pass runs no tests, lint, installer execution,
runtime checks, or screenshots; build and release proof does not establish them.
The DeviceWeb production target installs its own frozen lockfile with ancestor
workspace discovery disabled, then runs its local Vite bundler directly. The
TypeScript projects use `noEmit`, so their separate type-check step is not needed
to produce the bundle and is excluded from this production release path.

Build and package producer timestamps use UTC explicitly. Historical local logs
from before this correction printed local clock values with a `Z` suffix and
must not be used as UTC release-timing evidence without a validated offset.
