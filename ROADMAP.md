# Roadmap

## Landed on `master`

- Establish semantic Material light/dark roles in the production native workspaces, including
  contextual brand, Preview, and Device schemes.
- Move the primary Prepare actions into a Material bottom bar with live sidebar spacing.
- Add the isolated libgit2-backed project-history core and focused tests for complete `.3mf`
  snapshots, ordered commits, safe restore, Save As identity migration, collision handling, and
  shutdown draining.
- Close the Windows Release NanoSVG/static-library dependency boundary needed by standalone native
  tests.
- Retain English, Hong Kong Cantonese preview (`yue_HK`), and compact bilingual-preview language
  modes, with English fallback and existing Bambu Studio locales.
- Retain the Windows installer, CycloneDX, checksum, attestation, immutable-release, and disposable
  runner validation gates already encoded in the workflows.

## Locally verified partial native modernization

- Release GUI and installed payload built locally; the installed DLL matched the Release output.
- Full-display compositor smoke captures were reviewed for real native Home, Filament Manager,
  Device's official plug-in gate, and File → Version history. This verifies those partial native
  surfaces only; it does not establish a completed or faithful MD3 rewrite. The history dialog
  showed two app-local Git snapshots after Save As.
- Focused final CTest passed: language mode, project-history shutdown drain, and deterministic
  BBS 3MF export (3/3). See `HANDOFF.md` for the explicit aggregate/libnest waiver.
- DeviceWeb's `js-yaml` audit issue was repaired in the production lock; local high-severity audit
  is clean. Hosted run `29806330072` remains the authoritative CI result until complete.

## Native integration follow-up

- Finish review and compile fixes for the real wxWidgets/OpenGL Material implementation:
  - Material top bar and menu structure;
  - responsive Prepare printer/bed/filament sidebar;
  - top scene commands and left vertical transform/gizmo rail;
  - Prepare plate/estimate/slice/print bottom bar;
  - contextual Preview dock, chips, timeline, and sequential-view layout; and
  - native Device Temperature, Print Options, AMS, and Move cards.
- Finish project-history lifecycle integration so each discrete edit boundary is staged before the
  next edit can replace its state, manual saves are captured exactly, shutdown drains pending work,
  and recoverable failures remain visible and retryable.
- Finish **File → Version history**, rollback-safe restore, Save As ancestry migration, and Material
  styling for the history dialog.
- Finish native Cantonese strings for the new Material/history surfaces and rerun placeholder,
  resource, and fallback checks.
- Rerun the authoritative hosted Release configure/build/install and focused CTest after the
  repair workflow completes.
- Repair the upstream aggregate and `libnest2d_tests` baselines, then restore their coverage to
  the hosted gate rather than retaining the focused waiver.
- Smoke the installed application with a fresh isolated `--datadir` through the available low-level
  desktop/computer-control integration. Cover import, separate edits, save, Save As, history listing,
  safe restore, slice, Preview, Device/plugin gate, theme, localization, and resize behavior.
- Inspect the app-local bare repository after the smoke and prove that separate edits are reachable
  as distinct complete `.3mf` revisions in strict order.
- Capture full-compositor screenshots of the real native Prepare, Preview, and Device surfaces,
  visually review them, and replace or supplement the README's clearly labeled `ui-md3` reference
  images.
- Split the remaining changes into reviewable commits, preserve the unrelated generated-route-tree
  change, push `master`, and monitor the resulting GitHub Actions run to completion.
- Replace all pending fields in `HANDOFF.md` with tested commit, build, test, smoke, screenshot, and
  hosted-run evidence.

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
