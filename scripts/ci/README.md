# Official Windows baseline diagnostic

`official-windows-baseline.yml` builds the official `v02.08.04.57` native
source on a GitHub-hosted `windows-2025` runner. It runs only by manual
dispatch, normally from `main`, and has no push trigger. A dispatched revision
whose native trees differ from the official tag stops at the baseline check
described below.
The portable ZIP, PDB files, build transcript, and source/build metadata are
short-lived workflow artifacts for diagnosis. They are not a release or a
supported installer. The workflow does not launch the GUI or open a project.

## Hosted bootstrap inventory

| Requirement | Source and check |
| --- | --- |
| Windows and SDK | `windows-2025`; choose an installed Windows 10 SDK include tree containing `winrt/windows.graphics.printing3d.h` |
| Visual Studio C++ | Hosted Visual Studio 2026 C++ toolset, discovered with `vswhere`; `microsoft/setup-msbuild` adds MSBuild to `PATH` |
| CMake and Git | Hosted tools, checked before configuration; Visual Studio 2026 requires CMake 4.2 or newer. The verified absolute CMake path and SHA-256 are recorded, then kept first on `PATH` for child tools after package bootstrap. |
| Chocolatey | Hosted package manager, checked before a missing package is installed |
| `pkgconfiglite` | Chocolatey version `0.28.0`, installed only when `pkg-config.exe` is missing |
| Strawberry Perl | Chocolatey package if missing; its Perl is put first on `PATH` and checked for `Locale::Maketext::Simple` |
| Native dependencies | Official `deps/` CMake superbuild, Release, Visual Studio 2026 x64; cache key includes `hashFiles('deps/**')` and the hosted Visual Studio 2026 runner identity |
| Device web build | Official CMake target performs its own Node.js and pnpm bootstrap from the versions in `src/slic3r/GUI/DeviceWeb/CMakeLists.txt` |
| Application | Official CMake install target, Release, with MSVC PDB generation enabled |

The existing distributor branch has a different `deps/` tree and is not used
as a cache source. A cache miss builds the official dependencies from source.
Native `src/`, `deps/`, and `resources/` must match the official tag for the
baseline. The script fails if any of those trees changes before reapplication.

The inherited cross-platform entry workflow is inactive during this
diagnostic. Its reusable files are left intact. No inherited release publisher
runs from this diagnostic.

## Published portable comparison

`official-runtime-baseline.yml` is a separate hosted diagnostic. It runs only
by manual dispatch, normally from `main`, and has no push trigger. It verifies
that the official `v02.08.04.57` tag peels to commit
`f977235e6d736c4c0b650520ac5a5b72cbfe9244`, checks the published ZIP's
size and SHA-256 against the vendor release metadata, and hashes the extracted
native executable. The driver attempts to open the repository's public
`flowrate-test-pass1.3mf` fixture on a named hidden desktop with a fresh
`--datadir`. It records owned process and window observations and tears down
only the process it can revalidate. Raw pixels, profiles, logs, and dumps are
kept off workflow artifacts. Because the vendor executable has no instrumented
model-state probe, a surviving window is explicitly not reported as proof that
the 3MF loaded. Only safe JSON metadata is uploaded.
