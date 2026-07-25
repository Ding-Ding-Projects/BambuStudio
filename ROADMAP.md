# Roadmap

## Landed

### Roadmap execution wave (2026-07-24, evening)

- **MD3 dialog chrome** (`Widgets/MD3DialogChrome`): borderless owned dialogs (Config
  profiles, Version history, Material color picker, Stop-print interlock) carry a 44px MD3
  caption — mnemonic-safe title, draggable strip (HTCAPTION), keyboard-reachable close with a
  44px target, DWM rounded corners — instead of the native Windows title bar. Remaining
  #32770 dialogs sweep continues as follow-up.
- **M3 motion** (`Widgets/MD3Motion`): duration tokens, standard/emphasized easing, an
  interruptible timer Anim that honours OS reduced motion. First adoptions: dialog entrance
  fades (all chrome'd dialogs + the command palette) and the SlideToConfirm animated
  snap-back (grab interrupts it).
- **Preferences auto-history** (`PreferencesHistory` + `AppConfig::set_save_observer`):
  every settings save schedules a debounced, deduped Git snapshot of BambuStudio.conf into
  the profiles-root engine; browse/restore via Config profiles ▸ Preferences history…
  (restore writes beside the live file, never over it).
- **AI printer watch** (`PrinterWatch`, opt-in, OFF by default): periodic live-view frame →
  local Ollama (`qwen2.5vl` default; Gemma 3 works; gpt-oss is text-only) → OK info toast or
  persistent PROBLEM warning with a fix suggestion. Localhost only, one request in flight,
  silent when Ollama or the stream is absent. Preferences ▸ Other section.
- **Design-folder register**: 128 done / 3 justified deviations — the scene-toolbar
  centering and Objects-card rows were stale (already landed in code) and were flipped with
  evidence; the remaining three deviations are deliberate (corner toasts per the standing
  notification rules) or blocked (webview host APIs, SyncAms shell — covered by the chrome
  sweep).
- **CI unblocked**: brace-expansion 5.0.8 override cleared the new GHSA-mh99-v99m-4gvg
  audit failure. Catalogs at 562, gate green.


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
  OpenCode search helper (clipboard prompt + local launch).
- **Material color picker + color translator** (`Widgets/MD3ColorPicker`): continuous S/V +
  hue picker with a per-hue Material tonal ladder and a live rgb/hsv/nearest-name translator,
  replacing the native colour dialog on the accent "+" tile. **Stop-print safety interlock**
  (`StopPrintGate`): two key switches → three double-press arming buttons → slide-to-confirm →
  lift-away hazard cover revealing the real STOP button, with a plain-language stage caption
  at every step. Catalogs at 547.

## Next: follow-ups

- Sweep the remaining native-caption (#32770) dialogs onto `MD3DialogChrome`, and extend
  `MD3Motion` to toast enter/exit and tab/page fade-through.
- Design-folder residuals: `project-webview-legacy-anatomy` (needs C++ host APIs) and
  `syncams-shell-image-panel-greys` (chrome sweep + tokenized compare-panel greys).
- Hardware verification passes: stop-print interlock and AI printer watch end-to-end with a
  connected printer (+ local Ollama vision model).

## Next: continue matching `ui-md3/design-system` (standing mandate)

The parity register is **128 done / 3 justified deviations / 0 open** (2026-07-24). The three
deviations are held deliberately: corner-anchored toasts (the standing notification rules
override the kit's bottom-center), the project webview (needs C++ host APIs), and the SyncAms
shell (picked up by the MD3DialogChrome sweep). Re-evaluate each when its constraint moves.
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
