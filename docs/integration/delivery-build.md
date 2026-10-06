# Isolated delivery builds

The supported commands remain `build.bat /s` and `build-installer.bat /s`.
Squirrel tooling is reused only after its retained NuGet archive matches the
pinned SHA-256 and every tool-file byte matches that archive. Legacy NuGet and
old tool caches remain untouched. Verified tools use a content-addressed owned
cache; invalid owned caches are preserved under a unique previous-cache path
before atomic directory promotion. An invalid cache without its ownership marker
stops preparation and remains intact. Ten fixture-archive assertions cover warm
reuse, tampering, missing and extra files, repair, and uncertain ownership without
executing any tool.
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
