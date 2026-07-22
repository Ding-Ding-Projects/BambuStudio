# Handoff

## Delivered evidence — 2026-07-21

This effort completed the Material Design 3 token and typography migration across the native GUI tree
and added two native features (a MakerWorld OpenGL model preview and a dockable Prepare sidebar). The
parity audit reports the color, token, and typography layer as complete; the remaining deltas are
structural component anatomy (camera-HUD overlay, Material Symbols icon-font infrastructure, some
pill-geometry variants). This is not yet a faithful component-by-component MD3 rewrite, and no
release success is claimed.

## Commits

- Repository: `https://github.com/Ding-Ding-Projects/BambuStudio.git`
- Branch: `master`.
- `origin/master` is at `a29f629c0` — "Ship the Material Symbols Outlined icon font" (the vendored
  variable TTF + Apache-2.0 license from google/material-design-icons, first commit of the
  structural-anatomy waves). Everything described in this handoff is pushed. Work-in-progress waves
  land on `claude/md3-structural-waves` in the `.claude/worktrees` build worktree and are pushed to
  `master` per completed wave.
- Key migration commits (all pushed): `23688c23d` (MD3 token parity with the vendored kit),
  `49a7c4d46` (UI to MD3 theme tokens and fonts), `2f968cbc1` (GUI colors to MD3 tokens),
  `2cebf9091` (Roboto Mono and mono type helpers), `e4b468c5f` (GUI colors to MD3 semantic tokens),
  `76db0d5c1` (SBOM repository identity), `c700c91b0` (immutable-probe 403 tolerance),
  `8d727d49d` (completion sweep + model preview + dockable sidebar), `ec631dfb2` (draft-release
  lookup fix + docs).
- The unrelated generated change to
  `src/slic3r/GUI/DeviceWeb/device_page/src/routeTree.gen.ts` lives only in the detached build
  worktree under `.claude/worktrees/`, not in this checkout. Preserve it there; do not fold it into
  migration commits.
- Merged and deleted task branches (ancestry proven against `origin/master` before deletion):
  `claude/bambu-studio-ui-migration-38d0ef`, `codex/build-and-test-lowlevel-mcp`,
  `codex/native-material-validation`, `codex/auto-install-build-dependencies`,
  `remote_branch_v12`. Retained: upstream `v1.x`/`release/*` branches and four historic branches
  with unmerged work that cannot be integrated safely (`SaltWei-patch-1`,
  `copilot/fix-ams-spinning-icon-issue`, `feature/libnoise-deps`, `bambu-pomfret/web-conflict`).

## Local build and tests

- Full local Release builds of the migrated tree succeed for both dependencies and the application
  (VS2022 BuildTools, Windows SDK 10.0.26100).
- Earlier focused gate (2026-07-20, commit `3b00dc6aa`): `language_mode_tests`,
  `project_history_tests`, and `deterministic_bbs_3mf_tests` — 3/3 passed. This is not full-suite
  coverage. Aggregate `libslic3r_tests` currently has upstream/API-drift compilation failures and
  `libnest2d_tests` has known baseline runtime failures; both remain intentionally waived from the
  focused gate pending upstream repair. Do not describe the gate as an aggregate-suite pass.

## Hosted CI and release status

- On the migrated tree the hosted `Build BambuStudio` job succeeds: run
  [`29848731027`](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/29848731027)
  (head `7a027fa26`) and the later run
  [`29862992010`](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/29862992010)
  (head `c700c91b0`, `origin/master`). Both overall runs are marked failure because the separate
  `Publish Windows release` job fails.
- **The publish pipeline is green.** Run
  [`29877040307`](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/29877040307)
  (head `ec631dfb2`, which contains the draft-release lookup fix and the `8d727d49d` feature commit)
  completed with **both** `Build BambuStudio` and `Publish Windows release` succeeding on
  2026-07-22Z. It published the non-draft release
  [`md3-windows-v02.08.01.55-r37`](https://github.com/Ding-Ding-Projects/BambuStudio/releases/tag/md3-windows-v02.08.01.55-r37)
  with `BambuStudioMD3-Setup.exe` (~208 MB), its `.sha256`, and the CycloneDX SBOM
  (`BambuStudioMD3.cdx.json`). The earlier SBOM-identity (`76db0d5c1`), immutable-probe 403
  (`c700c91b0`), and draft-visibility (`ec631dfb2`) fixes are all verified by this run. The model
  preview and dockable sidebar are therefore pushed, built, and shipped in that installer.
- Authenticode provisioning remains external work; GitHub attestations and SHA-256 checksums stand in
  for it in the published release.

## Native smoke and screenshots

The installed application was launched with an isolated `--datadir` and a real STL on 2026-07-20, and
full-display compositor captures were visually reviewed. These captures predate the full token sweep;
they are evidence of native modernization only, not `ui-md3` reference images and not proof of full
component-anatomy conformance:

- `docs/readme-assets/native-material-home-light-en.png` — native Home.
- `docs/readme-assets/native-material-filament-manager-light-en.png` — native Filament Manager,
  signed-out state.
- `docs/readme-assets/native-material-device-plugin-gate-light-en.png` — native Device official
  plug-in gate; no plug-in installation was performed.
- `docs/readme-assets/native-material-project-history-light-en.png` — native **File → Version
  history** showing two local Git snapshots for `material-history-smoke.3mf`.

Fresh captures of the fully migrated Prepare, Preview, and Device surfaces are still pending.

## Project history (unchanged behavior)

The native app includes app-local, Git-backed version history for `.3mf` projects. Each retained
revision is a complete project snapshot in an isolated bare repository below Bambu Studio's data
directory, never a `.git` directory beside the user's project. `project_history_tests` includes the
shutdown-drain cases: stopping is admission-only, accepted work drains (including bounded external-lock
waits) before the worker joins, and a lock held by another process can delay shutdown up to the
existing timeout rather than being silently cancelled. History stays local to the device: it is not
pushed to the source repository, not synced, and not a backup, and there is not yet a
retention/pruning policy.

## Deferred work

- ~~Push local `master` (`8d727d49d`) and obtain a hosted CI run~~ — done; verified by green run
  `29877040307` and published release `md3-windows-v02.08.01.55-r37`.
- ~~Achieve a fully green publish run~~ — done; same run and release as above.
- **Structural-anatomy waves implemented (multi-agent session, 2026-07-21/22).** All four waves are
  written and committed on the build worktree branch:
  - `ae690fa85` (pushed) — Cantonese strings for the model-preview and Prepare-dock surfaces:
    yue_HK + English catalogs, rebuilt `.mo`, refreshed `coverage.json`, and new
    `language_mode_tests` assertions for standalone-Cantonese and bilingual modes.
  - `c4417d883` — Material Symbols icon-font infrastructure: private registration of the bundled
    TTF in `Label::initSysFont`, the `MaterialIcon` helper (28 cmap-verified PUA glyphs,
    availability probe, font factory, wxDC draw/measure, antialiased bitmap producer), CMake
    wiring, and an AxisCtrlButton proving site with bitmap fallback. Only the default
    Outlined/wght400/FILL0 instance renders through wxFont; active state is expressed via colour.
  - `be90ec1f0` — Device camera-HUD strip per the kit camera-card anatomy: always-dark `CameraHUD`
    band with a visibility-aware pulsing LIVE badge (`MD3::Viewport::live`), migrated status
    indicators pinned to on-dark bitmaps, and icon-font settings/fullscreen chips preserving the
    old CameraItem event wiring. Sizer sibling above the video — nothing overlays the native
    `wxMediaCtrl` HWND.
  - `f9005d609` — pill/literals closure: `MD3::Metrics::pill_radius(height)`; verified the shared
    Widgets pills are already DPI-safe (`applyMD3Style`/`Rescale`); each residual bitmap-bound
    theme literal is anchored and justified in `docs/features/design-system/md3-design-system.md`
    rather than unsafely tokenized.
  **All four waves are pushed** (`origin/master` = `a924a9f1f`). The three C++ commits passed the
  local incremental Release build gate first: 0 errors, "All steps completed successfully",
  41 minutes, MaterialIcon/CameraHUD/StatusPanel compiled and linked (only the pre-existing
  LNK4098 warning). Twelve Opus agents (plan → implement → 3-lens adversarial review → fix)
  produced them; the review's one blocker (wxBitmapBundle absent from the vendored wx 3.1.5) was
  fixed before commit. Hosted CI runs for these pushes were in progress at the time of writing; the
  local gate did not build the test binaries, so `language_mode_tests` for the new Cantonese
  assertions is proven by CI, not locally. Known follow-up: the HUD's `_L("LIVE")` badge string
  still needs catalog entries (en identity, yue_HK) and a `.mo` rebuild.
- Capture and review fresh full-compositor screenshots of the fully token-migrated native surfaces
  and replace the pre-sweep captures above.
- Repair/re-enable the aggregate and `libnest2d_tests` suites instead of relying on the focused waiver.
- Complete dark-theme/Cantonese native smoke and independent Cantonese review, including the new
  model-preview and sidebar surfaces.
- Add retention/pruning/quota controls and user-controlled backup/export for local project history.
- Provision Authenticode with a trusted signing identity; GitHub attestations and SHA-256 checksums do
  not satisfy this.
