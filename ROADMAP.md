# Roadmap

## Landed

All items below are committed and pushed on `master`. Commit `8d727d49d` (native model preview,
dockable Prepare sidebar, and the last migration-coverage changes) is pushed, built, and shipped:
hosted run `29877040307` (head `ec631dfb2`) completed fully green — including the previously failing
`Publish Windows release` job — and published the non-draft release
`md3-windows-v02.08.01.55-r37` (installer, SHA-256, CycloneDX SBOM).

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
- Finish the remaining pill-geometry variants. The shared Widgets library is done — every kit "pill"
  (Button, SwitchButton, SideButton call sites) derives its radius from height / 2 at paint/layout,
  DPI-safe, now named as `MD3::Metrics::pill_radius(height)`; segmented controls are deliberately not
  pills. What remains is feature-level chrome/settings controls with no dedicated widget class:
  filter/choice chips, the search-field pill, and the settings nav-item pill.
- The three theme literals retained over fixed bitmap assets are now anchored and justified in
  `docs/features/design-system/md3-design-system.md` ("Retained theme literals"): the assembly-tree
  delete badge (`AssemblyStepsUtilsImgui.cpp:4646-4647`, bound to the light-baked `cross_dark.svg`)
  and the Helio header banner (`HelioReleaseNote.cpp:3168-3169`, bound to the `helio_icon` brand
  bitmap). They are intentional until the icon-font / brand-asset infrastructure lets baked glyph
  colors be tinted from tokens. Two further intentional retentions — the coupled preview-timeline
  step marker (`AssemblyStepsUtilsImgui.cpp:823`/`:835`) and the amber "unsaved view" dot
  (`:4606`, which needs an amber/warning role) — are tracked as token-parity follow-ups.

### Verification and delivery

- ~~Push local `master` and obtain a hosted CI run~~ — done: run `29877040307` (head `ec631dfb2`).
- ~~Complete a fully green hosted run that also publishes the immutable release~~ — done: the same
  run published non-draft release `md3-windows-v02.08.01.55-r37` with installer, SHA-256 checksum,
  and CycloneDX SBOM; the draft-visibility failure was cleared by the lookup fix in `ec631dfb2`.
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
