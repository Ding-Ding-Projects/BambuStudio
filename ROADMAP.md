# Roadmap

## Landed

### Screenshot refresh, light/dark legibility fixes, search everywhere, config profiles & backup (2026-07-24)

- Replaced the complete `docs/screenshots` matrix from the current build (headless Mesa
  llvmpipe + PrintWindow for native surfaces, headless Edge for the Home/Wizard webviews),
  including the rewritten advanced regex-builder popover and the new Calibration nav tab.
- Light-mode Preview legibility fixed: the gcode legend dock now pushes a themed
  `ImGuiCol_Text` (OnSurface), so per-filament values, filament-change/cost and time
  estimation are no longer white-on-light. The Preview move bar sheds its skip buttons and
  then its counter on narrow widths instead of sliding the handle over the readout.
- Search bars everywhere: version history and the print-host upload queue gained the shared
  MD3 `SearchField` (regex builder included); the Plater object search became colour-aware
  (`SearchField::colorSearchText` — match by `#RRGGBB` or nearest common colour name).
- New **File ▸ Config profiles & backup…**: export the entire data directory (secrets
  included) into one archive, import it on another PC as a new profile, unlimited profiles
  with one-click launch (`--datadir`), and per-profile local Git snapshot history driven by
  the project-history engine — all gated behind a keyboard-operable slide-to-confirm control
  with an explicit secrets warning. 43 new curated Cantonese catalog entries (494 total).
- Ctrl+F **command palette**: every enabled menu command (icon + description), navigation
  targets, and rich inline quick-settings rows (theme / density / accent) behind one shared
  SearchField query. The regex builder gained a tabbed **Reference** mini-documentation
  (per-token descriptions from the live chip tables, engine details, examples) plus an
  OpenCode search helper (clipboard prompt + local launch). Catalogs at 530.

### Dark-mode legibility, advanced regex builder, software-GL self-heal (2026-07-24, `e2ed70365`)

- Root-caused and fixed the systemic dark-mode text corruption (non-idempotent
  `darkModeColorFor` double-remap via hex-aliased tokens), plus the action-bar white squares,
  the malformed pill radii, the Process-header switch overlap, orange category glyphs, dim
  field values, and the grey caption band.
- Replaced the minimal search "tune" popover with a full guided regex builder
  (`Widgets/RegexBuilderPopup`): literals/classes/anchors/groups/quantifiers sections, raw
  pattern editor, live syntax feedback, sample-text testing with capture groups, copy, engine
  identification; bounded and backtracking-guarded. Wired working regex into every search
  surface, including the previously-inert object-list search and both ImGui filters.
- Releases now self-heal on machines without OpenGL 2.0: hash-pinned Mesa llvmpipe 26.1.3
  ships in the installer's `mesa\` folder and the app relaunches itself once onto software
  rendering instead of dying at the GL gate.
- Release pipeline repaired: the tag-triggered release loop is dead (branches-only trigger +
  ref guard + echo purge), old releases cleared, and the Windows CI build moved to
  Ninja + sccache with `SLIC3R_MSVC_PDB=OFF` (CI ships no PDBs) for warm-cache speed.

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

### Structural component anatomy (register waves 1–9)

Nine implementation waves driven by the parity register
(`docs/features/design-system/md3-parity-register.md`) are committed and pushed on `master`, each
gated by review and shipped through the hosted pipeline (releases `r37` through `r53` published
along the train). Landed highlights: the Material Symbols icon font and the ImGui
Roboto/Mono/Material-Symbols atlas with the raster-to-glyph sweeps; the rebuilt shared widget kit
(SearchField, Slider, Checkbox, Radio, Switch, segmented controls, chips, fields); the `MD3Dialog`
borderless shell with the MessageDialog family, the leaf-dialog reparenting, and the
raw-`wxMessageBox` sweep; the kit title bar; the Preferences NavRail with runtime density and
accent-seed controls; the device camera HUD, temperature rows, print options, AMS card reskin, and
farm card grid; the Preview timeline transport bar; the glyph-to-GL-texture bridge plus toolbar and
gizmo-rail chrome; and the sidebar object-manipulation card.

### Build and release tooling

- Support pinning the Windows SDK via `PS_WINSDK` in `build_win.bat` and `deps-windows.cmake` as a
  partial-SDK MSB8037 workaround.
- Bind the SBOM generator to `pkg:github/$GITHUB_REPOSITORY` so the release identity is correct.
- Make the immutable-release settings probe tolerate HTTP 403 and rely on post-publish
  immutability verification instead of failing.
- Rebuild the NSIS installer on MD3 (custom Welcome/language/install-source/build-progress/Finish
  pages, documented Win32 deviations, and the UTF-8 `/INPUTCHARSET` fix for the previously garbled
  Cantonese language page), and add the interactive build-from-source install mode
  (`3c12a1771`; see `docs/features/releases/windows-build-from-source.md`).
- Authenticate the release-publish step with the `TOKEN_GITHUB` owner PAT, falling back to the
  workflow token where the secret is absent (`fc7257366`). This works around an org-side
  restriction that began returning HTTP 403 on release creation with the workflow token; `r56` was
  published manually from run artifacts during that incident.

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

### Structural component anatomy (from the parity register)

The canonical tracker is `docs/features/design-system/md3-parity-register.md` — **120 done / 4
recorded deviations / 5 open** after Wave 9 (2026-07-22). The register is the live source of
truth; the counts here are a snapshot.

- The 5 open rows are the deep Prepare-sidebar rebuilds that wrap live-bound widgets — printer
  identity card, bed SelectField collapse, filament info-rows, Process card, Objects card. Each
  needs an implement-build-verify loop against the live preset/printer combos; a concurrent
  implementation wave is finishing them.
- The 4 recorded deviations each carry concrete evidence in the register: the Device XY dial kept
  as a 3x3 grid with a 10/1 step selector (the dial encoded jog magnitude in hit radius), the
  scene-toolbar pill reskinned but not re-centred (collides with the collapse toolbar), the SyncAms
  partial shell (simplebook footer gating), and the project-webview page (host-injected read-only
  page restyled to kit tokens/CSS; true file-manager anatomy needs C++ host APIs).
- A small set of bitmap-bound theme literals remains anchored and justified in
  `docs/features/design-system/md3-design-system.md` ("Retained theme literals") pending tintable
  brand-asset infrastructure and an amber/warning role.

### Verification and delivery

- ~~Push local `master` and obtain a hosted CI run~~ — done: run `29877040307` (head `ec631dfb2`).
- ~~Complete a fully green hosted run that also publishes the immutable release~~ — done: the same
  run published non-draft release `md3-windows-v02.08.01.55-r37` with installer, SHA-256 checksum,
  and CycloneDX SBOM; the draft-visibility failure was cleared by the lookup fix in `ec631dfb2`.
- Capture and review fresh screenshots of the fully migrated native Home, Prepare, Preview, and
  Device surfaces under the canonical filenames
  `docs/readme-assets/native-md3-{home,prepare,preview,device}-light-en.png`. Until those captures
  are produced and reviewed, the README gallery keeps the reviewed pre-sweep `native-material-*`
  captures rather than referencing images that do not exist; the gallery switches to the canonical
  filenames only when the files actually land.
- Verify the installer's build-from-source mode end-to-end on a real machine. It compiles and is
  reviewed, but its first complete interactive run (toolchain bootstrap through installed payload)
  has not happened yet, and `PRODUCT_SOURCE_REPO_URL` defaults to a placeholder the owner should
  confirm.
- Wire the repaired test suites back into hosted CI. Wave 9 ported the drifted PrusaSlicer config
  keys to BambuStudio names and fixed the invalid Catch2 `[NotWorking]` exclusion, but the isolated
  suite build did not finish in-window; `libslic3r_tests` and `libnest2d_tests` remain waived from
  the hosted gate until that wiring lands.
- Adopt `MD3::Metrics::active()` at the remaining (~40) metric call sites so a density change
  applies live instead of being restart-scoped.
- Preserve the unrelated generated `routeTree.gen.ts` change when splitting the remaining work into
  reviewable commits and pushing `master`.

### Project history and localization

- Project-history retry semantics shipped during the register waves: durable failure notification
  with Retry, retained failures surfaced in the history dialog with per-item and bulk retry, and
  orphaned-manifest adoption on restart. Remaining lifecycle work is confirming each discrete edit
  boundary is staged before the next edit can replace its state under real editing load.
- Cantonese catalogs were kept current through the waves (model-preview, sidebar, Material,
  history, and error-flow strings catalogued; the `.mo` reproducibility `--check` gate is green).
  Remaining: strings from the in-flight sidebar wave, rerunning placeholder/resource/fallback
  checks after it, and the independent human review of Cantonese copy tracked below.

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
