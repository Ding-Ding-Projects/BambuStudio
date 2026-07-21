# Roadmap

## Landed

All items below are committed on `master`. The final commit `8d727d49d` (native model preview,
dockable Prepare sidebar, and the last migration-coverage changes) is on local `master` but was not
yet pushed to `origin/master` (`c700c91b0`) at the time of writing, so it has no hosted CI run yet.

### Material Design 3 token and typography layer

- Extend `src/slic3r/GUI/Widgets/MD3Tokens.hpp` to full parity with the vendored
  `ui-md3/design-system/` kit: the `OnError`/`OnErrorContainer`/`InversePrimary` roles, scrim and
  shadow tints, the `elev1`–`elev5` elevation ladder, `MD3::Viewport` axis and live colors, fixed
  panel/dialog/content metrics and shape radii, the full 11-step `MD3::Type` scale with font
  constants, and the `accentFromSeed()` seed-ramp port (commit `23688c23d`).
- Convert hardcoded theme colors and fonts across essentially the whole GUI tree in six waves
  (roughly 120 files): the shared Widgets library and the ImGui theme; chrome and status bars;
  Prepare/Plater; the preview renderer and timeline; gizmos and viewport overlays; Device,
  StatusPanel, AMS, DeviceTab, and multi-machine surfaces; Settings, parameters, and Search; the leaf
  dialogs including calibration; residual files; the Project webview CSS; and the Home webview
  (verified). Conversions use `StateColor::semantic` / `ThemeColor` / `MD3::resolve`.
- Ship Roboto and Roboto Mono under `resources/fonts`, registered privately at startup by
  `Label::initSysFont`, and expose the `Label::Mono_*` faces for numeric and technical values.
- Resolve contextual schemes per workspace: brand green (Prepare and general UI), Preview purple, and
  Device teal.
- Preserve functional data colors (filament swatches, G-code feature colors, 3D paint palettes),
  which carry meaning and were intentionally left untouched.

### Native features

- Add a native OpenGL model preview for the MakerWorld "Download and Open" flow
  (`src/slic3r/GUI/ModelPreviewDialog.hpp`/`.cpp`): an orbit/zoom/fit GL canvas in an MD3 dialog,
  hooked pre-import in `Plater::import_model_id`, with **Open in Prepare** / **Close** actions and a
  failure-safe fallback to the normal import.
- Add a dockable Prepare sidebar driven by `wxAuiManager`: app-config key `prepare_sidebar_dock`
  (`left`|`right`|`top`|`bottom`, default `left`), live re-dock from a Preferences "Prepare panel
  position" control, DPI-correct, preserving collapse and float behavior.

### Build and release tooling

- Support pinning the Windows SDK via `PS_WINSDK` in `build_win.bat` and `deps-windows.cmake` as a
  partial-SDK MSB8037 workaround.
- Bind the SBOM generator to `pkg:github/$GITHUB_REPOSITORY` so the release identity is correct.
- Make the immutable-release settings probe tolerate HTTP 403 and rely on post-publish
  immutability verification instead of failing.

### Earlier landed work (retained)

- Establish semantic Material light/dark roles in the production native workspaces, including
  contextual brand, Preview, and Device schemes; move the primary Prepare actions into a Material
  bottom bar with live sidebar spacing.
- Add the isolated libgit2-backed project-history core and focused tests for complete `.3mf`
  snapshots, ordered commits, safe restore, Save As identity migration, collision handling, and
  shutdown draining.
- Close the Windows Release NanoSVG/static-library dependency boundary needed by standalone native
  tests.
- Retain English, Hong Kong Cantonese preview (`yue_HK`), and compact bilingual-preview language
  modes, with English fallback and existing Bambu Studio locales.
- Retain the Windows installer, CycloneDX, checksum, attestation, immutable-release, and disposable
  runner validation gates already encoded in the workflows.

## Remaining

### Structural component anatomy (from the parity audit)

The color, token, and typography layer is reported complete; the remaining deltas are component
anatomy, not mis-colorings.

- Build the camera-HUD overlay system for the viewport.
- Add the Material Symbols icon-font infrastructure so the `Material Symbols Outlined` token can back
  real icon glyphs instead of the existing bitmap assets.
- Finish the remaining pill-geometry variants.
- Resolve the three theme literals currently retained intentionally over fixed bitmap assets (the
  final scan found 21 residual literals: 18 addressed, 3 kept over fixed bitmaps).

### Verification and delivery

- Push local `master` (commit `8d727d49d`, the model preview and dockable sidebar) and obtain a
  hosted CI run for those two features; the `Build BambuStudio` job passes on the already-pushed
  migrated tree (runs `29848731027` and `29862992010`).
- Complete a fully green hosted run that also publishes the immutable release. After the SBOM-identity
  and immutable-probe (HTTP 403) fixes cleared their earlier failures, the publish job currently
  still fails at the draft-release visibility stage; publication is not yet verified.
- Capture fresh full-compositor screenshots of the fully token-migrated native Prepare, Preview, and
  Device surfaces, visually review them, and replace the README's pre-sweep captures.
- Preserve the unrelated generated `routeTree.gen.ts` change when splitting the remaining work into
  reviewable commits and pushing `master`.

### Project history and localization

- Finish the project-history lifecycle integration so each discrete edit boundary is staged before
  the next edit can replace its state, manual saves are captured exactly, shutdown drains pending
  work, and recoverable failures remain visible and retryable.
- Finish native Cantonese strings for the new Material, history, model-preview, and sidebar surfaces
  and rerun placeholder, resource, and fallback checks.
- Repair the upstream aggregate and `libnest2d_tests` baselines, then restore their coverage to the
  hosted gate rather than retaining the focused waiver.

## Needed before calling Material/history complete

- Confirm localized Preview chips and statistics remain usable at narrow widths and do not occlude
  the sequential G-code view.
- Confirm top-bar navigation, Preferences, Prepare plate state, gizmo highlighting, DPI changes,
  light/dark changes, mouse capture, and tooltips remain wired to production behavior.
- Confirm the Device cards preserve the official networking-plugin gate; do not bypass or simulate
  that production boundary for screenshots.
- Confirm restore never overwrites the current project implicitly, Save As retains ancestry without
  joining unrelated histories, and capture/commit failures surface a durable recovery message.
- Document local-history storage and privacy prominently: no cloud sync, no source-repository commits,
  no retention/pruning yet, path-based identity, full-snapshot disk growth, and no replacement for
  ordinary backups.
- Obtain independent human review of Cantonese copy for print safety, destructive actions,
  account/privacy, recovery, and networking flows.

## Later or externally blocked

- Add an explicit history quota, retention/pruning controls, repository maintenance, export/import,
  and optional user-controlled backup or synchronization. None of these are part of the current
  local-history implementation.
- Add deterministic native screenshot baselines, pixel-difference thresholds, OCR/glyph checks, and
  broader keyboard/accessibility traversal after the initial real-app smoke is stable.
- Configure Authenticode with a trusted signing identity/provider and publish the certificate and
  rotation policy. GitHub artifact attestations and SHA-256 checksums do not satisfy this item.
- Complete Cantonese coverage and ongoing linguistic QA. Formal `zh_TW` must remain written
  Traditional Chinese and must not be treated as a Cantonese substitute.
