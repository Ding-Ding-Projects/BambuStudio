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
- `origin/master` and local `master` are in sync at `ec631dfb2` — "Fix draft-release lookup and
  refresh project documentation". Everything described in this handoff is pushed.
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
- The publish failure has moved. The SBOM-identity fix (`76db0d5c1`) and the immutable-probe HTTP 403
  tolerance (`c700c91b0`) cleared their earlier failures — the settings probe now warns and relies on
  post-publish verification instead of failing. In run `29862992010` the "Publish uniquely tagged
  Windows release" step now fails later, at the draft-release visibility stage (repeated
  HTTP 404 "release is not visible yet", then an exception). A fully green run that also publishes the
  immutable release has **not** yet succeeded; do not claim it did.
- The two new features (model preview, dockable sidebar) live in local commit `8d727d49d`, which is
  not yet pushed and therefore has no hosted run.
- No installer, SBOM, checksum, release, attestation, or Authenticode success is claimed. Authenticode
  provisioning remains external work.

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

- Push local `master` (`8d727d49d`) and obtain a hosted CI run for the model preview and dockable
  sidebar.
- Achieve a fully green publish run: the current blocker is the draft-release visibility failure in
  the publish step, not the already-fixed SBOM-identity or immutable-probe issues.
- Complete the structural component anatomy from the audit: camera-HUD overlay, Material Symbols
  icon-font infrastructure, remaining pill-geometry variants, and the three theme literals retained
  over fixed bitmap assets.
- Capture and review fresh full-compositor screenshots of the fully token-migrated native surfaces
  and replace the pre-sweep captures above.
- Repair/re-enable the aggregate and `libnest2d_tests` suites instead of relying on the focused waiver.
- Complete dark-theme/Cantonese native smoke and independent Cantonese review, including the new
  model-preview and sidebar surfaces.
- Add retention/pruning/quota controls and user-controlled backup/export for local project history.
- Provision Authenticode with a trusted signing identity; GitHub attestations and SHA-256 checksums do
  not satisfy this.
