# Handoff

## Delivered branch evidence — 2026-07-20

The captures and source changes document partial native UI modernization informed by Material
Design 3. They do not establish a completed or faithful MD3 rewrite.

- Repository: `https://github.com/Ding-Ding-Projects/BambuStudio.git`
- Delivery branch: `codex/build-and-test-lowlevel-mcp`
- Renderer prerequisite: `b4561feaa821d159ac54ef9bb166dff85f4239ae`, cherry-picked as `9c4991c26`.
- Current implementation/verification commit: `3b00dc6aaa2da82e724e4d4d44281813e1071787`.
- Local DeviceWeb dependency repair: `pnpm install --lockfile-only --no-frozen-lockfile` updated the pinned `js-yaml` override and lock to 4.3.0; `pnpm audit --audit-level high` reported no known vulnerabilities.

## Local build and tests

- The locally configured Release GUI target and `install` target completed successfully. The installed payload is `install_local\\bambu-studio.exe`; `BambuStudio.dll` matched the Release build SHA-256 `E2A9D5F65183B7B86A9698C1E83B938553070B4D56C97AE69B047BED569A13B6`.
- Focused CTest on the final source changes: `language_mode_tests`, `project_history_tests`, and `deterministic_bbs_3mf_tests` — **3/3 passed** in 4.08 s.
- `project_history_tests` includes the shutdown-drain cases. The lifecycle repair makes stopping admission-only: accepted work drains, including bounded external-lock waits, before the worker joins. A lock held by another process can therefore delay shutdown up to the existing timeout; it is not silently cancelled.
- Explicit coverage limitation: this is not full-suite coverage. Aggregate `libslic3r_tests` currently has upstream/API-drift compilation failures (marching squares, Voronoi, and legacy 3MF APIs). `libnest2d_tests` has known baseline runtime failures (ArrangeRectanglesLoose, PrusaParts, EmptyItem SIGSEGV). The branch workflow intentionally gates maintained focused tests only; do not describe it as an aggregate-suite pass.

## Native smoke and screenshots

The installed application was launched with an isolated `--datadir` and a real STL, then full-display compositor captures were visually reviewed. They are evidence of partial native modernization, not `ui-md3` reference images or proof of full MD3 conformance:

- `docs/readme-assets/native-material-home-light-en.png` — native Home.
- `docs/readme-assets/native-material-filament-manager-light-en.png` — native Filament Manager, signed-out state.
- `docs/readme-assets/native-material-device-plugin-gate-light-en.png` — native Device official plug-in gate; no plug-in installation was performed.
- `docs/readme-assets/native-material-project-history-light-en.png` — native **File → Version history** showing two local Git snapshots for `material-history-smoke.3mf`.

The project-history smoke used Save As, then opened Version History. No restore or destructive operation was performed. History stays in an app-local private Git repository, not beside the project and never in the source repository.

## Hosted CI and release status

- Previous run `29802840504` failed before C++ tests because DeviceWeb audit detected the high `js-yaml` advisory.
- Repair push run: `https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/29806330072` (status must be rechecked before claiming CI success).
- No installer, SBOM, checksum, release, attestation, or Authenticode claim is made until that replacement workflow completes successfully. Authenticode provisioning remains external work.

## Deferred work

- Repair/re-enable the aggregate and libnest2d suites instead of relying on the focused waiver.
- Complete dark-theme/Cantonese native smoke and independent Cantonese review.
- Add retention/pruning/quota controls and user-controlled backup/export for local project history.
- Confirm hosted installer/SBOM/checksum/attestation evidence after a successful workflow.
