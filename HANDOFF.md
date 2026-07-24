# Dark-mode/clipping wave + advanced regex builder + release-pipeline repair (2026-07-24)

User-reported: "hardly visible text everywhere" (dark mode), Process-card overlap, "latest
release app not launching", release list flooded/mis-ordered, "not all search bars have regex
builder", and a mandate for a fully advanced regex builder with documentation. Four parallel
Fable agents (disjoint owners) + orchestrator; one serial build gate; headless dark-mode
verification via the DarkQA recipe (see lowlevel-mcp-headless-driving memory — cross-desktop
hwnd access now requires launching the cheap CLI ON the headless desktop).

- **Release loop root-caused/killed (`0d4091229`, `6be213882`):** `on: push: {}` also matched
  the tags the release job itself created → every release re-built the same commit under a
  stale title (142 releases, newest not on top). Push trigger is branches-only now, release
  job refuses tag refs, in-flight tag-echo runs cancelled, 140 old releases deleted (tags
  kept; keepers: r192, r174, portable-preview-1).
- **"App not launching" root-caused:** r192's payload launches fine (extracted + verified
  headlessly with Mesa). Fresh installs die at the OpenGL<2.0 gate on GPU-less machines.
  Fix shipped in-tree: hash-pinned Mesa llvmpipe 26.1.3 (pal1000 mesa-dist-win, byte-identical
  to the locally proven DLLs) staged into the installer's mesa\ subfolder + OpenGLManager
  one-shot self-relaunch (copy-beside-exe, BBS_SOFTGL_RETRIED triple loop-guard), uninstaller
  handles the runtime copies, docs/features/windows/software-gl-fallback.md.
- **Dark-mode systemic fix:** StateColor::darkModeColorFor was applied twice on many paths and
  the map was not idempotent — MD3::Dark::onSurface was hex-identical to a light key, so
  correct dark text got remapped to near-black (#2f3036 on dark surfaces; pixel-verified).
  Fixed by 1-step hex nudges severing the aliases (+ invariant comments), TextDisabled dark
  legibility bump, StaticBox stale-parent-bg refresh (white squares behind rounded controls),
  SideButton radius clamp (the green "eggplant" blob), action-bar disabled-text derivation,
  SwitchButton min-size (Process "bal Obj" overlap), TabCtrl glyph pinning + Tab.cpp fallback
  fix (orange segment glyphs), Label dark seeding, frame bg + topbar width sync (grey band).
- **Advanced regex builder (user mandate):** new Widgets/RegexBuilderPopup.{hpp,cpp} — guided
  sections (literals auto-escape, classes, anchors, groups/alternation, quantifiers + lazy),
  raw pattern editor (bidir sync), flags, live validity with friendly std::regex_error text,
  collapsible Test area (sample text, highlighted matches, capture groups), copy, engine
  caption (std::regex ECMAScript); bounded (2000-char pattern / 20000-char sample / 200
  matches, all guarded). docs/features/windows/regex-builder.md rewritten.
- **Regex on every search surface:** object-list search's builder was inert (search_object
  ignored flags — now routed through textMatches + live re-filter, highlight index bug fixed);
  Tab preset pill and Ctrl+F SearchDialog now host real SearchFields; device picker on the
  shared matcher; ImGui font picker + assembly tree gained guarded `.*` toggles.
- **CI speed:** per-ref cancel-in-progress; Windows app build moved to Ninja + sccache (GHA
  cache). First canary failed on C1041 (parallel cl racing the shared /Zi PDB) — root fix:
  the one live /Zi (root CMakeLists add_compile_options) is now /Z7, which also makes objs
  sccache-cacheable. Re-validated by the wave push's CI run.
- **Catalogs:** 451 yue_HK translations (+68 regex builder, +2 softgl), .mo --check green,
  coverage.json updated. PO escape gotcha recorded: the parser is ast.literal_eval, so \b and
  mesa\opengl32.dll must be double-backslashed in msgid/msgstr.

**Verification (completed):** serial local Release build green (1h11m, 0 errors, DLL relink
proven against object timestamps). Headless dark-mode recapture PASS on every reported defect:
"Not sliced" + both action pills legible, white square gone, option segments proper capsules,
Layer-height/manipulation values legible, Global/Objects header with zero overlap, neutral
segment glyphs, dark caption full-width. The residual half-clipped preset-row glyph was
root-caused to the sizer-less DISABLE_UNDO_SYS undo-to-sys button floating at the panel origin —
hidden (`f8461434e`), re-verified by crop. Light-mode regression pass clean on the same surfaces.
Wave pushed as `e2ed70365` (38 files) + `f8461434e`; summary in Discussion #2; wiki Releases page
updated.

**Release-loop postscript:** the loop RESURRECTED after the trigger fix — tag-triggered runs
execute the OLD workflow snapshot at the tag's commit, so surviving echoes kept re-seeding
(~25 more releases). Contained by cancelling every tag-ref run and arming a session reaper
(cancels new tag-ref runs within 2 min); echoes starve because none reaches publish. 165 echo
releases deleted in total; keepers r192/r174/portable-preview-1. Any future session that sees
`md3-windows-*`-ref runs should cancel them on sight until the old tags age out.

**CI state at handoff-write:** final wave run 30081955084 (head `f8461434e`, Ninja+sccache path
with SLIC3R_MSVC_PDB=OFF) in progress — NOT yet claimed green; its release will be the first
containing this wave. First sccache run only populates the cache; warm timing evidence needs the
run after it. e2ed70365's run was cancelled by design (per-ref cancel-in-progress supersede).

# Post-conformance feature + polish program (2026-07-23)

With the parity register closed (127 done / 4 recorded deviations / 0 open), this session delivered
the user's feature/quality backlog on top. Each wave: parallel disjoint-owner edits, then ONE serial
build (to avoid concurrent-build MSBuild-tracker corruption), then adversarial review; visual waves
verified by rendering the real app.

**Verification harness (new).** This VM has no GPU, so BambuStudio gated at OpenGL<2.0 and never
reached its UI. Fix: Mesa llvmpipe `opengl32.dll` beside the exe + `GALLIUM_DRIVER=llvmpipe`. The app
is now driven/captured fully HEADLESS via the `lowlevel-computer-use-mcp` cheap CLI (off-screen
CreateDesktop + PrintWindow) — see the `lowlevel-mcp-headless-driving` memory + `scratchpad/ll-drive.sh`.
This uncovered + fixed the real startup crash: the Material Symbols variable TTF through GDI+
(wxGCDC/wxGraphicsContext) corrupted the heap (PageHeap-verified at `GdipCloneFontFamily`); MaterialIcon
now renders exclusively via plain GDI and the bundled faces register session-visible (not FR_PRIVATE).
CI-verified (`7b6ea27f9`). A portable-ZIP preview (`portable-preview-1`) was published from a
launch-verified local build. The CI publish HTTP 403 (org-side, flapping) is worked around by the
`TOKEN_GITHUB` PAT in `build_all.yml`; releases auto-publish again (r94..r101+).

**Features shipped (all pushed to master):**
- Clipping + accessibility (`bca31b40b`): left-edge bleed-through root-caused to the docked AUI sidebar
  pane border; app-wide keyboard focus/activation + accessible names + focus rings, contrast, unit
  suffix + HMS width clipping. (The checkbox/radio 44px hit-target was reverted — it regressed layout
  app-wide; row-level hit targets are the followup.)
- MD3 first-run Setup Wizard + Home hero (`50dee281e`): guide webview restyled to MD3 tokens (region
  list an outlined card, not a flat green bar), GuideFrame on the MD3Dialog shell, Home hero given a
  leading brand title instead of an empty green band.
- Regex builder on every search bar (`66c807f54`): SearchField `.*` toggle + tune builder popover
  (token chips, case/whole-word) + shared `textMatches()`, wired into Preferences, preset editor,
  user-presets, device farm, global option search, and the ImGui in-canvas search; guarded against
  catastrophic-backtracking throws at every site.
- Full UI font customization (`f4ed83643`): Appearance Font family + Text size, runtime-applied via the
  Label factory (`rebuild_fonts`) with CJK-safe fallback and a live preview.
- Browser-like project tabs + grouping (`04a02a676`): new MD3 `ProjectTabBar` (one tab per project,
  drag-reorder, colored groups, per-tab dirty dot, custom-font labels) as session file-tabs over the
  single Plater (switch = save-outgoing-if-dirty + load-selected, reusing the Backup/Restore round-trip).
  A spike confirmed true multi-live-project is a ~1200-call-site rewrite (out of scope). Known: a switch
  costs a file-open (full deserialize) + resets transient undo/camera; followups — orphan temp-snapshot
  sweep on restart, never-saved-tab Save-As identity, command-line-opened file getting its own tab.

Catalogs at 368 yue_HK translations (.mo --check green).

**Non-blocking notifications wave (2026-07-23, `ab007cda2`):** the three central OK-only funnels
(`show_info` / `warning_catcher` / `show_error` in GUI.cpp) now route to NotificationManager corner
toasts (Regular/Warning/Error), covering ~120 informational call sites; modal fallback when a modal
dialog is on top or the Plater isn't up yet. Decision dialogs untouched. Full-build gate 0 errors;
mirrored into the user's global agent instructions (agent-global-memory `e71ece3`) alongside
appearance-editor, external-editor, and local-version-control mandates.

**External editor + appearance completion wave (2026-07-23, in tree, GUI-lib compile green):**
- `Utils/ExternalEditor.{hpp,cpp}` (registry+PATH detection, table transcribed from the
  desktop-material reference), `open_in_external_editor` launcher, Preferences ▸ General editor
  combobox + Custom path row, File ▸ "Open in External Editor" with warning-toast fallback.
- Appearance: free accent color picker (wxColourDialog into the same `setAccentSeed` pipeline),
  "Reset appearance to defaults" (under a reentrancy guard — `MultiSwitchButton::SetSelection`
  EMITS its event), live MD3 token preview panel; `AccentSwatch::colour()` keeps preset rings
  truthful on matching custom picks.
- Hardening: CheckBox.hpp docstrings corrected (20/18px reality), Label.cpp SectionHeader GC-font
  guard made effective (strict `IsValidFacename`, cached — `faceIsInstalled`'s probe fallback echoes
  any name on MSW and never fails).
- Adversarial review (3 agents): 2 FAIL verdicts → all MEDIUM findings fixed in-tree (reset
  reentrancy, dead GC guard); LOWs fixed (custom-editor silent fallback, swatch rings).
- Feature docs: new `docs/features/workspace/` category (notifications, version history, project
  tabs, external editor) + `windows/appearance-customization.md` + `windows/regex-builder.md`.

**Screenshot matrix (complete, `9f450f86d` + follow-up):** `docs/screenshots/<feature>/` — 202
captures, one per page and one per button per feature. Webview surfaces (Home, Wizard) captured via
headless Edge against `resources/web` HTML (WebView2 never composes on a headless desktop); native
surfaces via PrintWindow, which — with Mesa llvmpipe — DOES capture the GL viewport + ImGui toasts
(memory `lowlevel-mcp-headless-driving` updated). Pass-2 verification against the rebuilt exe, all
headlessly measured: toast bottom-RIGHT anchoring PASS (17px right margin), toast fully visible
above the plate/slice bar PASS (root cause: notification base was SLIDER_DEFAULT_BOTTOM_MARGIN=10
while the native bar overlaps the canvas bottom ~66px — new NOTIFICATION_DEFAULT_BOTTOM_MARGIN,
final value 80 for a 14px breathing gap; 80 is geometry-derived from the measured 64-flush capture,
compile-gated but not yet re-captured); presets-up-to-date modal→toast PASS (no modal in a 39-frame
watch); External-editor rows present in General (VS Code auto-detected) PASS; appearance
Custom…/reset/MD3-preview present PASS; Other-tab toggle spacing PASS. Known cosmetic followups:
Preview legend column alignment — RESOLVED (`809a230d6`): reproduced the sliced-cube Preview
headlessly and found the FILAMENT|MODEL value columns were already aligned (header + value share
`offsets_`); the real defect was the grey "Filament change times" / "Cost" summary rows crammed
directly under the table. Fixed by wrapping them in the table's own ItemSpacing.y (6*m_scale) plus
a spacer row so they get the same row advance; before/after headless crops confirm the summary
rows now clear the value row. The Custom-accent-button edge clip was FIXED in-session (replaced with a 32px "+" tile
matching the swatch geometry; headlessly re-verified, screenshots refreshed). The CI failure on
9f450f86d/94f72d916 was root-caused to the Cantonese catalog gate: entries were added to the
.po/.mo without updating bbl/i18n/yue_HK/coverage.json — fixed via the official
compile_translation.py (381 validated translations, deterministic .mo, Test-LanguageModes.ps1
passing locally). NOTIFICATION_DEFAULT_BOTTOM_MARGIN finalized at 80px (14px gap above the bar).
The captured File menu now truthfully shows "Open in External Editor" (disabled on an unsaved
project — its enable-condition at work).

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

## Register closed — final structural wave (2026-07-23)

The parity register is **127 done / 4 recorded deviations / 0 open**. The final build-in-the-loop
wave closed the five audit-reopened rows: the Process card gained a SectionHeader(tune) + a
process-preset SelectField wrapping the live PlaterPresetComboBox + a Quality/Strength/Support/Others
SegmentedControl that filters curated rows (this fixes the cramped/overlapping Process header the
visual QA caught); the Filament title moved onto the literal SectionHeader class; ReleaseNote's five
stock-chrome siblings and ProgressDialog reparented onto the MD3Dialog shell (which gained an
additive two-phase CreateShell path — the 32 other subclasses are untouched). The objects-searchctrl
row is a recorded deviation: the selected-row SecondaryContainer chip landed, but the per-cell type
glyphs + visibility/checkbox anatomy need a change in ObjectDataViewModel (outside the wave's owned
files). Review verified behavior preservation and no GDI+ font regressions; all TUs compiled and
libslic3r_gui.lib re-archived. Reused existing catalog strings only.

**Verification harness this session:** the startup crash (variable-font-through-GDI+ heap corruption)
is fixed and pushed; a Mesa llvmpipe software-GL DLL beside the exe lets the GPU-less VM render the
real UI, and the lowlevel-computer-use-mcp cheap CLI drives + PrintWindow-captures it fully headless
(see the ci-is-free and lowlevel-mcp memories). Live visual QA found: the fixed Process header (now
addressed), a left-edge ~15px bleed-through clip, the legacy Setup Wizard/GuideFrame, and the Home
hero — plus a 71-item clipping+a11y audit (25 high). Those, the regex-builder-on-every-search-bar,
full font customization, and browser-like project tabs are the queued follow-on waves.


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
  assertions is proven by CI, not locally. The LIVE-badge follow-up is closed: "LIVE" is in the
  en catalog, yue_HK renders it 直播中 (connection-offline category), coverage.json counts 288,
  the shipped `.mo` was rebuilt with `bbl/i18n/yue_HK/compile_translation.py` and its `--check`
  reproducibility gate passes locally, and `language_mode_tests` asserts the key in standalone
  and bilingual modes. A scoped audit confirmed no other string from the wave commits is missing
  catalog entries.
  **Next program (in flight): full MD3 conformance.** The user has mandated that the entire UI
  match `ui-md3/design-system/` with zero original design elements (functional data colors
  exempt). A 10-surface Opus audit is generating
  `docs/features/design-system/md3-parity-register.md` — the canonical open-gap register and
  wave plan; implementation proceeds register-wave by register-wave (each build-gated and pushed),
  folding in the already-scoped feature-pill (CapsuleButton 5px→pill, Tab search field 5px→pill),
  camera-HUD temp-chip, and project-history durable-retry slices.
  **Register Wave 1 is implemented** (17 Opus implement groups, zero unfinished assignments; 3-lens
  review found 4 findings — 3 fixed, 1 verified false positive): 22 register rows done and 2 partial
  (the Preview section-header `palette` and status-pill `layers` glyphs wait on the Wave 2 ImGui
  Material-Symbols atlas), plus the scoped extras — CapsuleButton chip pill, Tab search-field pill,
  and CameraHUD nozzle/bed temp chips. The new Preview status string "Sliced · %1% layers" is
  catalogued (en + yue_HK 切好片喇 · 共 %1% 層, coverage 289, `.mo` rebuilt, `--check` green).
  Wave 1 is pushed (`70bd80309`). **Delivery policy per user 2026-07-22: ship-first — waves push
  right after implement+review; hosted CI is the build verification and failures are fixed forward;
  local builds run only as informational checks.**
  **Progress through the register (59 done / ~5 partial / 67 open):** Wave 2 shipped (`bd1788b48`
  glyph enum ~126 verified codepoints, `67d3079b5` ImGui Roboto/Mono/Material-Symbols atlas);
  Wave 3 subset shipped (`d8960fa96`, raster→glyph on eight surfaces; review fixed three HiDPI
  dpiRef threads and a glyph-semantics swap); Wave 4 shipped (`4341bc4f1`, Preview overlay + slider
  on the atlas, both former partial rows closed); Sprint A shipped (`6a2f3e346` shared-widget
  library rebuild incl. new SearchField/Slider widgets, `7bbff238f` Slice/Print + tab controls +
  preset search, `0a061e984` HMS/MediaPlay/pause-stop, plus the project-history durable-retry
  commit) — all 10 Sprint A groups delivered every assigned gap; reviews across these waves found
  only isolated defects, all fixed (one false-positive exclusion alarm was my own atlas edit).
  The recurring infra annoyance is StructuredOutput retry-cap failures on some final agent reports;
  work always landed and was recovered from journals — later reviews use plain-text output.
  Project-history retry semantics shipped: durable failure notification with Retry, retained
  failures surfaced in the history dialog with per-item and bulk retry, orphaned-manifest adoption
  on restart; new error-flow strings catalogued (en + yue_HK, coverage 294, `.mo` `--check` green).
  **Sprint B shipped** (`3aa4bb972`, release `md3-windows-v02.08.01.55-r53`): the ten-group glyph
  and anatomy sweep — MainFrame/Plater/StatusPanel icon slices, SideTools signal draw,
  FilamentGroupPopup anatomy, Slice/Print leading glyphs, snackbar recolor, and every formerly
  glyph-blocked row; 16 gaps, review traced every symbol clean, two verified defects fixed.
  Register stood at 73 done / 56 open. CI green down the entire train (r37–r53 releases published).
  **Wave 7 shipped (this push, 2026-07-22): register now 108 done / 21 open.** The prior session
  hit its usage limit mid-Wave-7; this session salvaged the marshal partition from the session
  export and relaunched — 14 Opus agents (10 disjoint-owner groups + read-only test-repair scout +
  2 adversarial reviewers + fixer). Landed: the `MD3Dialog` borderless shell primitive and the
  whole MsgDialog family + 10 leaf dialogs reparented onto it; the `GLIconGlyphBridge`
  glyph→GL-texture bridge routing the 3D-editor toolbar and gizmo rail to Material Symbols
  (capability-gated, SVG fallback intact); title bar to kit anatomy (brand tile, history chip,
  project chip, appearance button; Save/Undo/Redo/Publish removed from the caption, Calibration
  re-homed as a text menu button); Preferences rebuilt (230px NavRail, new Appearance section with
  Theme/Density/Accent controls — density/accent persist but await runtime wiring); Preview
  timeline transport bar; device farm list→card grid; StatusPanel section headers + Z/extruder
  glyphs; Prepare-sidebar safe subset. Reviewers found 3 real defects, all fixed: SendToPrinter
  and PublishDialog header-X paths bypassed job teardown (both now route the original close
  handlers), plus a `-Wreorder` cleanup. Three agents' final reports died on the recurring
  StructuredOutput retry-cap; their edits landed and were recovered from transcripts. New strings
  catalogued: 12 en entries, 21 yue_HK entries (coverage 315, `.mo` rebuilt, `--check` green).
  Local informational Release build gate passed (0 errors, exe + dll relinked).
  During the ship a parallel push by codingmachineedge (`ef6fd59f2`/`156f9dd2d`) landed a
  hand-rolled variant of the same device-section-headers row; the wave was rebased onto it and
  the reviewed kit-SectionHeader version supersedes it at the tip (the parallel commits remain
  ancestors; their register edit had introduced a corrupted duplicate row, repaired here).
  **Test-repair re-scope (scout, read-only) — premise correction:** the CI waiver is pure omission
  (only 3 targets built/run); `libslic3r_tests` has NO statically-provable compile blocker — the
  provable drift is RUNTIME: PrusaSlicer config keys removed in BambuStudio (`perimeters`→
  `wall_loops`, `first_layer_height`→`initial_layer_print_height`, etc.) throw or null-deref in
  `test_config.cpp`/`test_placeholder_parser.cpp`; `libnest2d_tests` compiles, and its
  `exclude:[NotWorking]` quarantine is invalid Catch2 syntax (would not apply). Executable repair
  plan recorded: port the config keys, fix the Catch2 exclusion to `~[NotWorking]`, optionally
  split a runtime-passing `libslic3r` subset into CI.
  **Installer overhaul shipped (`3c12a1771`):** the NSIS installer is restyled to MD3 (custom
  Welcome/language/install-mode/build-progress/Finish pages, documented D1–D7 Win32 deviation
  list) and the mojibake language page is fixed at the root (UTF-8 BOM + `/INPUTCHARSET UTF8` on
  all five makensis calls — the Cantonese strings were being read as CP-1252). A new
  build-from-source mode bootstraps Git/Node.js/VS Build Tools, installs opencode, and builds the
  release source with a bounded five-cycle fully non-interactive opencode auto-repair loop
  (blanket-allow config scoped to the clone, question stays deny); the build page is non-closable
  while the build runs. Review hardening before ship: `FileReadUTF16LE` for manifest/status reads
  (plain `FileRead` truncated UTF-16LE, which would have made from-source uninstalls delete
  nothing), and a declined prebuilt fallback can no longer advance into INSTFILES with a partial
  payload. Compiles at makensis 3.12 EXIT=0 locally; silent-mode CI fixtures unchanged.
  From-source is interactive-only and never runs in CI; its first end-to-end run on a real
  machine is still an open verification item, and `PRODUCT_SOURCE_REPO_URL` defaults to a
  placeholder the owner should confirm.
  **Wave 8 shipped: register 113 done / 16 open.** Eleven Opus agents (marshal + 8 groups + 2
  reviewers + fixer; both reviews CLEAN, fixer verified with zero edits). Landed: the
  `raw-wxmessagebox` sweep (22 live sites across 8 files onto the MD3 MessageDialog with exact
  return-code preservation; early-boot/fatal-handler sites deliberately left native with reasons
  recorded), preset-editor NavItem pills via a new TabCtrl leading-glyph API, device temperature
  rows (teal glyphs + mono values + trailing edit IconButton, live AMS humidity wired), GL
  gizmo-rail/scene-toolbar background chrome plus the viewport zoom cluster and object stat pill,
  filament subtitle row folded into its SectionHeader, four new cmap-verified MaterialIcon glyphs,
  and runtime Density/Accent application (`MD3::Metrics::active()` + accent-seed override;
  density is restart-scoped until ~40 call sites adopt `active()`). Catalogs at 322 (`--check`
  green).
  **Wave 9 shipped (completion wave): register 120 done / 4 recorded deviations / 5 open.**
  Ten Opus agents, both reviews CLEAN, fixer applied one density nit. Done: the sidebar
  object-manipulation card (read-only live mirror of the gizmo cache, 250ms timer), device
  print-options (4-way speed SegmentedControl, teal fan Sliders, chamber-light Switch), the
  Control-strip removal into an overflow menu, the AMS card reskin to Device-teal tokens (every
  load/unload/RFID/tray signal preserved), the gizmo-rail kit anatomy (44px r12 tiles,
  Primary-fill selected, group dividers), and TextureImport + Helio onto new additive MD3Dialog
  resizable / forced-dark variants. Recorded deviations, each with concrete evidence in the
  register commit: XY dial→3x3 grid + 10/1 step selector (dial encoded magnitude in hit radius —
  one-gesture jog becomes two-step), scene-toolbar pill reskinned but not re-centred (collides
  with the collapse toolbar), SyncAms partial shell (simplebook footer gating), and the
  project-webview (host-injected read-only page restyled to kit tokens/CSS; true file-manager
  anatomy needs C++ host APIs). Test repair: config keys ported to BambuStudio names, the invalid
  Catch2 exclusion fixed; the isolated suite build did not finish in-window — CI wiring deferred.
  New strings catalogued (coverage 331, .mo --check green).
  **Waves 10-14 shipped (2026-07-22, parallel worktrees): THE REGISTER IS CLOSED — 125 done /
  4 recorded deviations / 0 open.** The combined Fable wave landed the five build-in-the-loop
  Plater rows (printer identity card, bed SelectField with relocated hover popup, filament
  info-rows with lockstep teardown, Process card with an Advanced/Simple flip keeping the full
  ParamsPanel reachable, kit Objects card), finished the aggregate-test repair (both suites
  compile and RUN: libslic3r_tests 87/95 pass, libnest2d 3 failures — the 11 residual runtime
  failures are algorithm drift, recorded and deliberately NOT wired into CI), migrated 21 density
  call sites to Metrics::active(), and polished MD3Dialog DPI / StateColor Device dark pairs /
  Helio siblings. Opus waves 12-13 delivered GL viewport polish (glyph sub-chrome with per-icon
  kept-raster reasons, identity-driven rail dividers, cached chrome with a review-caught
  texture use-after-free fixed) and SendToPrinter/calibration/device-farm completion. Fable wave
  14 wired startup density/accent, added Preferences live search, refreshed README/ROADMAP/docs
  indexes and the GitHub wiki (6b0747d). Catalogs at 348, .mo --check green throughout.
  **CRITICAL open verification — startup crash:** the Wave 14 screenshot pass found the
  Wave 8/9-era binary crashes with heap corruption before any window (6/6 launches, WER evidence
  recorded). That binary was built while agents were mid-edit, so the evidence is tainted; a
  clean incremental build of the final tree was launched and a runtime smoke on it is the
  mandatory next gate (the staged capture scripts make the screenshot retry fast). Until that
  smoke passes, no runtime-health claim is made for the shipped tree.
  **Startup crash root-caused and fixed (2026-07-22/23, user-reported 'App not launching'):**
  deterministic heap corruption (0xc0000374) during MainFrame construction, WinDbg-dumped to
  MaterialIcon::bitmap -> wxGDIPlusContext: the Material Symbols face is a VARIABLE TTF
  (fvar/gvar) and rendering it through GDI+ corrupts the heap (GDI is fine). Fix: MaterialIcon
  glyphs are now rasterized exclusively with plain GDI (shared glyph_image core; draw/measure/
  bitmap/bitmapPx all GDI+-free) and every gc->SetFont(MaterialIcon::font(...)) site
  (CheckBox/RadioBox/CameraHUD/AxisCtrlButton/StatusPanel/BBLTopbar) composites pre-rendered
  bitmaps. Debug-heap masking (_NO_DEBUG_HEAP=1) and a poisoned MSBuild tracker (stale objs/libs
  silently skipping compile+link, which also hid a get_extruder_color_icons signature drift)
  prolonged the hunt; local builds in this worktree now force-link before trusting results.
  **Sonnet conformance double-check (15 agents): 114 done-rows re-verified, 51 findings
  adversarially confirmed, 24 fixed + 2 partial in-tree** (ScoreDialog/MultiMachinePickPage onto
  the shell, SideTools/TempInput/ProgressBar de-legacied, star-rating + picker-checkbox glyphs,
  preview polish, swatch ring geometry, i18n literals). Register truth-reconciled to
  **123 done / 3 deviations / 5 open**: the stale XY-grid deviation flipped to done (the grid IS
  implemented), three over-claimed rows reopened (Process card completion, ObjectList row
  anatomy, section-header literal-class swap), and two audit-discovered rows added (ReleaseNote
  sibling shells, ProgressDialog shell). Those five need one build-in-the-loop follow-up wave.
  A locally built portable-ZIP preview release ships once the fixed binary passes the launch
  stress test; CI releases continue per push via the TOKEN_GITHUB path.
- Capture and review fresh full-compositor screenshots of the fully token-migrated native surfaces
  and replace the pre-sweep captures above.
- Repair/re-enable the aggregate and `libnest2d_tests` suites instead of relying on the focused waiver.
- Complete dark-theme/Cantonese native smoke and independent Cantonese review, including the new
  model-preview and sidebar surfaces.
- Add retention/pruning/quota controls and user-controlled backup/export for local project history.
- Provision Authenticode with a trusted signing identity; GitHub attestations and SHA-256 checksums do
  not satisfy this.
