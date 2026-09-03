# Roadmap

## In progress

### Native and embedded GUI accessibility wave (delivery verification — 2026-07-30)

- Shared native controls, the Filament Manager DeviceWeb page, Project resources, setup guides,
  focused contracts, and maintainer documentation are implemented on the delivery branch.
- Focused native source contracts, changed-file DeviceWeb lint, TypeScript compilation, accessibility
  tests, and the Vite production build have passed locally.
- Real MSVC compilation exposed and repaired the source-contract gaps: `SwitchBoard` now attaches its
  two-radio accessible peer, uses wxWindow's enabled state, sizes from translated labels, emits full
  command metadata, and activates Space/Enter once on the matching key-up instead of alternating under
  auto-repeat. The AMS settings gear now uses the command-event signature its shared Button emits.
- The post-repair focused Release library compilation and full `BambuStudio_app_gui` link both exit 0.
  The exact post-key-pair DLL is 150,811,136 bytes, timestamped `2026-07-30 00:44:51 -04:00`, with
  SHA-256 `1EECBBFFBB5AB87AF2A90050220E3B4A93E816291F5C29DF4276078CABF22530`.
  Lowlevel MCP runtime evidence is accepted only from that build; older intermediate captures are not.
- **Still required before moving this item to Landed:** exact-final-build Lowlevel MCP headless captures,
  default-branch push and remote ancestry proof, and factual hosted workflow/release links. `HANDOFF.md`
  is the evidence ledger.

### Home Assistant printer handover (implementation complete; verification pending — 2026-07-28)

- **Token-backed handover:** the Smart home dialog collects accessible printers, presents a
  No-default credential disclosure, calls `bambu_lab.add_printer` once per printer, and reports
  exact success/partial/failure results asynchronously. Calls run in four-wide waves, so a full
  32-printer batch occupies at most eight 30-second timeout waves instead of 32 serial timeouts.
- **Token transport safety:** every Home Assistant bearer request now requires HTTPS, except for
  HTTP to localhost or an explicit IPv4 loopback address. Clear-text LAN HTTP is rejected before a
  socket is opened; redirects and verbose protocol tracing are disabled for credential-bearing
  requests.
- **Token-free discovery:** the opt-in, non-persisted sharing toggle starts a fresh-capability
  `GET /bambustudio/printers` endpoint on one LAN interface and advertises it as
  `_bambu-slicer._tcp.local.` for at most five minutes. Disabling the toggle, closing the dialog,
  or reaching expiry stops serving and sends an mDNS goodbye.
- **Bounded credential surface:** authentication happens before printer data is requested; method,
  path, headers, time, concurrency, payload size, printer count, and field lengths are bounded.
  Tokens, access codes, payloads, and supplier exception details are not logged.
- **Bounded runtime work:** four/64 service execution, single-flight four-wide printer import and
  entity refresh, a serialized transactional light-restoration worker, 200 ms volume coalescing,
  authenticated HTTP rate limiting, cached sanitized offers, and paced/coalesced mDNS replies
  replace per-action detached-thread and amplification paths. Entity-state bodies stop at 4 MiB,
  parsing stops at 512 results, rendering stops at 256 matching rows, and persisted entity lists
  inspect at most 256 segments/64 KiB/256 bytes per value while retaining at most 32 unique values.
  A failed two-attempt light restore retains its generation-specific scene for manual recovery
  instead of deleting the only recovery path; cancellation after scene creation but before any
  flash deletes the unused scene, with a focused regression protecting that hand-off window.
- **Clipping and target repair:** the native dialog is resizable and work-area capped, scrolls long
  content behind a fixed footer, wraps dynamic/bilingual copy and rigid action rows, and exposes
  44-DIP controls. The shared confirmation dialog wraps its separate “don't show again” footer and
  stacks actions when width is constrained. The Pages landing header and Material You band reflow
  through 200% zoom; translated header links have explicit 44-pixel width and height floors so
  Linux CJK glyph metrics cannot shrink a target below the accessibility minimum.
- **Documentation and verification harness:** the categorized API contract, focused and master
  Postman collections, focused C++ tests, a production-service cross-host probe, and hosted
  workflow targets are present.
- **Native build and clipping evidence:** the full Release GUI target completed in 3,387 seconds
  and its first no-change rebuild completed in 8.3 seconds. English captures at 720×760 and the
  declared 520×480 minimum exposed text actions being squeezed to 44 DIP. The responsive-action
  repair rebuilt and linked in 141 seconds, its no-change rebuild took 8.0 seconds, and the reviewed
  before/after Close and media-action captures show the correction. The
  subsequent nonvisual import-scheduling and cancellation-cleanup fixes compiled and linked in
  214.808 seconds, followed by an 8.544-second no-change build. The final 151,299,584-byte DLL is
  timestamped `2026-07-28 08:15:46 -04:00`, with SHA-256
  `41BB1BFC754E3184C5908E2145A93E3640D3866E59380F32EEFF7A76F418E972`. The two primary corrected
  captures were recaptured from that exact DLL; the media-action close-up remains from the
  layout-identical `EBF646…` build.
- **Still required before moving this item to Landed:** native bilingual Smart home capture, Path B
  against a real Home Assistant, a real Home Assistant Path A confirmation card, physical-printer
  success, and issue #16 closure with exact evidence. The implementation and follow-up target fix
  are pushed and remotely proven; hosted Pages run `30359493216` passed 156/156 as the matrix stood on 2026-07-27 (it is 447 cases now), while the Windows
  release verdict remains maintained separately in issue #16. The full Release
  GUI build, native English 720×760 and 520×480 review, focused Release targets,
  30-case/267-assertion native suite, 5/5 focused CTest entries, 718-entry localization check, the
  static Pages/i18n/clipping checks and the browser width/zoom/language matrix as they stood that
  day (21/21 and 156/156; now 50 and 447), synchronized template assembly, and cross-host discovery/fetch/goodbye probe are complete. The
  synthetic TEST-NET probe is transport evidence, not a real-printer success claim.

## Landed

> Entries here are a dated record of what shipped, written in the vocabulary of the day. Waves that
> predate 2026-07-26 therefore say "filament" and "AMS" where the shipped UI now reads "ink" and
> "Ink Dispenser" — the rename is display-only and changed no identifier, so the entries stay as
> written rather than being retold. See
> [Ink terminology](docs/features/windows/ink-terminology.md).

### The published UI kit became self-contained (2026-07-28)

- Removed the last three third-party requests on the site. The design-system UI kit at
  `/app/design-system/ui_kits/bambu-studio/` had been loading React, ReactDOM and
  `@babel/standalone` from unpkg and compiling its own JSX in the browser on every visit; with
  unpkg unreachable it rendered nothing at all.
- **Vendored** React 18.3.1 and ReactDOM 18.3.1 locally (MIT, licence included) and **removed the
  Babel runtime entirely** by compiling the JSX at build time.
- Wrote the assembler the kit's header had credited for years without it existing:
  `assemble-ui-kit.mjs --check|--write`, plus `jsx-transform.mjs`, a dependency-free JSX compiler
  that throws on anything outside the supported subset instead of guessing. Its output is
  byte-for-byte identical to Babel's for all twelve sources.
- Fixed a latent `const` collision between `Components.jsx` and `App.jsx` that Babel's `const`→`var`
  rewrite had been hiding, and taught the assembler to fail the build on any such collision.
- Closed the hole that let this survive: the layout gate's third-party assertion now sweeps **every**
  published page rather than only the landing page, and a new suite loads the composed site in
  headless Chrome with every off-site host blackholed and requires it to render.

### GitHub Pages site rebuilt as a tabbed Material 3 app (2026-07-28)

- Replaced the single scrolling landing page with a **browser-style tabbed site** of eight tabs:
  a persistent strip with an overflow menu, drag and keyboard reordering, pinning, user-assignable
  groups, a searchable tab list, and order/pinning/grouping persisted across restarts.
- Added **English and Hong Kong Cantonese copy at five funny levels each**, on two independent
  persisted sliders. Facts are invariant across levels and that invariance is asserted per variant.
- Added the **full ECMAScript regex builder** — guided parts, flags, live matches with correct
  capture-group identity, copy and export — and wired it to every search bar on the site. Opt-in
  regex filtering runs inside a terminable Web Worker; where none can be created the toggle is
  disabled and plain text keeps working.
- Added a **changelog viewer** covering every published release (38 at the time of writing), which CI
  regenerates at deploy time from the Releases API
  plus the commits between tags, with a UTC calendar range filter, typed ISO and locale dates,
  composing search, and a Markdown export that states the range it exported.
- Added **non-blocking notifications** with a history centre, a **settings surface** with
  per-element appearance editors and a cross-tab search, and the **10% dim sum surprise** drawn from
  published assets in the public dim-sum photo catalog, with no opt-out.
- Gated the deploy on **444 measured runtime layout cases** (156 landing + 288 per-tab) plus 50
  static contracts; `compose-site.mjs` and `serve.mjs` make the published tree reproducible locally.
- Fixed **29 defects** found by a twelve-agent adversarial review of the new site, and the
  prototype's accessibility defects reported in issue #24.
- Documented in [`docs/features/pages/`](docs/features/pages/README.md); captures in
  [`docs/screenshots/pages/`](docs/screenshots/pages/README.md).

### Image-led app and GitHub Pages showcase (2026-07-27)

- Added eleven original, web-optimized visuals covering the landing hero, every app screen card,
  and social sharing.
- Upgraded the GitHub Pages landing page from CSS-only motifs to responsive editorial artwork with
  lazy feature cards, accessible descriptions, reduced-motion behavior, and Open Graph metadata.
- Reused the visual language inside the interactive app's Home welcome panel and recent-project
  cards, while keeping every action and localized label as live HTML.
- Extended the Pages composition and layout assertion so root-level landing assets cannot silently
  disappear during deployment.

### Chrome-sweep tranche: stock dialogs adopted (2026-07-25)

- **`MD3DialogCaption::Adopt()`** — one-call adoption for dialogs that cannot change base
  class: strips the native caption styles in place, wraps the existing root sizer under the
  44px caption strip (title falls back to the window title), restores the content client
  height, preserves `wxRESIZE_BORDER`, and finishes rounded corners + fade.
- **17 dialog classes de-natived** with it: AMS dryness control, AMS materials setting (+
  official-filament dialog), Print options + printer parts, Send print job (+ its two inline
  timelapse/storage dialogs), Save preset, Flushing volumes, Object color import, Full
  compare + Compare presets, sending-failed confirm, Keyboard shortcuts, About + Copyrights,
  System info, and Slic3r's own SingleChoiceDialog.
- **Stock wx prompts routed to chromed dialogs**: the four raw `wxSingleChoiceDialog` sites
  (choice index helper, Helio printer pick, both Config-profiles snapshot pickers) now use
  the chromed SingleChoiceDialog; the object rename `wxGetTextFromUser` became an adopted
  `wxTextEntryDialog`; two raw `wxMessageDialog`s (snapshot confirm, mixed-color sublayer)
  became Slic3r `MessageDialog`. Startup-error `wxMessageBox`es stay native deliberately —
  they fire where the MD3 shell may not be constructible.
- Headlessly verified on the fresh build: Keyboard shortcuts and About render the MD3
  caption with no native bar and intact content (`docs/screenshots/dialog-chrome/`).

### Smart-home & scanner wave (2026-07-24, late night)

- **AI filament scanner** (File menu): QR-code phone upload (token-guarded LAN server,
  vendored MIT qrcodegen), local Ollama vision identification, automatic AMS-slot
  configuration + best-preset auto-selection (brand-mapped onto the shipped vendor profile
  families), HUGE flashing on-screen announcement + optional TTS.
- **TTS narrator** (off by default, serialized, cooldown-limited, errors never suppressed):
  printer state changes and error codes, local SAPI voice + Home Assistant speakers.
- **Home Assistant integration** (Smart home dialog): entity browser with search bar +
  listbox, media-player rich controls (prev/play-pause/next/volume), announcement-speaker
  selection, alert lights with red-on-error / green-on-finish flashes protected by a
  generation-specific scene snapshot, two restore attempts, and retention of that scene for manual
  recovery when both restore attempts fail.


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

- Chrome sweep is COMPLETE for plain dialogs: three 2026-07-25 tranches adopted all 70
  adoptable stock dialogs (completion scan verified — see Landed). Deliberately excluded:
  10 popover-style/complex windows flagged for designed treatment (FilamentPicker shaped
  popover, fan/humidity popups, CommandPalette overlay, SettingsDialog frame, ParamsDialog,
  BedShapeDialog build_dialog, ObjectTableDialog positioning, ZUserLogin webview,
  RecenterDialog done with paint offset) and 7 dead classes. Follow-ups: FeedDirectionDialog
  caption doesn't track its dynamic `SetTitle` (shows static "Confirm");
  ManualNozzleCountDialog title literal "Set nozzle count" was never localized upstream.
- Motion: dialogs, palette, popovers and SlideToConfirm animate now; toast enter/exit is
  ImGui-native already — remaining candidate is tab/page fade-through (needs compositing).
- The ObjColor compare-panel greys are tokenized (SurfaceContainer roles); SyncAms itself was
  already on the MD3Dialog shell.
- Hardware verification passes: stop-print interlock and AI printer watch end-to-end with a
  connected printer (+ local Ollama vision model). Two matrix stragglers stay documented:
  toast-try-slice (needs a sliceable state) and the external-editor row close-ups (old
  captures remain accurate at row level).

## Next: continue matching `ui-md3/design-system` (standing mandate)

**Second-pass sweep (2026-09-02)** — recorded in the register under "Second-pass sweep: generic
elements and vocabulary": every user-visible string is rewritten at the translation boundary so
filament reads "ink" and the AMS reads "Ink Dispenser" (`GUI/LanguageMode.cpp` `vocabulary()`);
the stock wxButton / wxGauge / wxStaticLine / wxHyperlinkCtrl / wxCheckBox / wxChoice / wxSlider
constructions and seven single-line wxTextCtrl inputs became their kit widgets (a new
`Widgets/LabeledCheckBox` row, `ProgressBar::Pulse()` for indeterminate gauges); six owned dialogs
still on native OS chrome adopted `MD3DialogCaption`. Still open there: the seven wxRadioButton
groups, the multi-line text views, the SmartHome list box, and the 175 static bitmaps that belong
to the icons-assets families. Pinned by the "second-pass sweep" tests in
`ui-md3/tests/md3-conversion-contracts.test.mjs`.

- [x] Verify the converted controls in the Windows build. Runs `33695532981` and `33696115604`
  completed successfully on 2026-09-03, including the application build, unsigned Squirrel
  package validation, asset upload, and immutable release publication.
- [ ] Capture and review the light and dark states of each converted surface. The compiler-only
  repair in issue #28 had no visible state and did not satisfy this separate capture item.

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
  identity card, bed SelectField collapse, ink info-rows, Process card, Objects card. Each
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
  has not happened yet. Release packaging now binds the current repository and exact workflow commit;
  the installer has no mutable branch or shared-version-tag fallback.
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

- Keep the one-click Windows build aligned with the hosted release pipeline whenever dependency,
  Mesa, SBOM, or NSIS packaging policy changes; add a disposable-runner end-to-end invocation once
  its several-hour cost is acceptable for scheduled CI.

- Add an explicit history quota, retention/pruning controls, repository maintenance, export/import,
  and optional user-controlled backup or synchronization. None of these are part of the current
  local-history implementation.
- Add deterministic native screenshot baselines, pixel-difference thresholds, OCR/glyph checks, and
  broader keyboard/accessibility traversal after the initial real-app smoke is stable.
- Configure Authenticode with a trusted signing identity/provider and publish the certificate and
  rotation policy. GitHub artifact attestations and SHA-256 checksums do not satisfy this item.
- Complete Cantonese coverage and ongoing linguistic QA. Formal `zh_TW` must remain written
  Traditional Chinese and must not be treated as a Cantonese substitute.
