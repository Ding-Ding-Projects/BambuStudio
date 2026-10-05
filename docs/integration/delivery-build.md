# Isolated delivery builds

The supported commands remain `build.bat /s` and `build-installer.bat /s`.
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
