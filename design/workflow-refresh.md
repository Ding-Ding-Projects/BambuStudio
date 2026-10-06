# Studio Atlas: complete native interface refresh

## Status and design route

This is a source-native design specification for the complete Bambu Studio application, including its product-owned embedded views. It is not a screenshot, a running prototype, a completed implementation, or evidence of visual parity. The standalone website is excluded. The 28 named surface groups in [the delivery ledger](native-feature-delivery.md) remain in scope, including every nested control and reachable state.

The [bounded built-review queue](workflow-refresh/built-review-queue.md) maps the nine remaining visual boundaries to existing entrypoints, source anchors, proposed tuples and explicit fixture limits. It records preparation only, not completed interaction or rendered acceptance.

The rollback baseline is `d048cfc3040a1566b78e03334f3871f1dd0144bb`. Authoring started from `8610aeeb49297ecda6a25747be125d5e4034b30d`, which also preserves the earlier feature ledger. Material Designer 0.21.550 has a separately verified package, but its live creation/export/handoff flow has not been exercised. Application launches are explicitly prohibited for this task. That authorization boundary prevents the preferred live route, so this specification uses the existing `ui-md3/design-system/` native kit and `MD3Tokens.hpp` as its sanctioned foundation. No Material Designer export is claimed.

The design may be reverted if the maintainer dislikes it. Review and revert only the design-only commits listed in [the change ledger](workflow-refresh/change-ledger.json), in reverse dependency order, retaining unrelated build, packaging, printer, AMS, slicing and data fixes. The baseline identifies the visual starting point; it is not permission to reset the repository to it. A reviewed `git revert <design-only-commit>` creates an auditable reversal. Inspect every mixed commit and preserve its functional hunks before considering a selective reversal. There is no automatic rollback, history rewriting, preset migration, or user-data reset.

## Visual direction

Studio Atlas replaces the previous continuous gray slab with three readable layers: quiet window chrome, clearly bounded work regions, and small high-contrast action islands. The plate and camera remain the largest visual anchors. Color identifies context, selection and state, never the sole meaning. Green remains Prepare/general, purple remains Preview, and teal remains Print/Monitor. The application keeps its existing identity and configurable logo.

The decisive changes are the porcelain/slate neutral ramp, rounded workspace navigation, one obvious page heading, section headers with helpful supporting text, consistent field/card anatomy, a clearly ordered Print readiness surface, and generous separation between technical editing and transport actions. This is a complete surface contract, not a navigation-only reskin. A surface that still uses the old component styling remains unfinished even when the new tab strip is present.

### Color roles

| Role | Light | Dark | Use |
| --- | --- | --- | --- |
| Surface | `#f7f9fc` | `#151c25` | Window and canvas surround |
| Surface container lowest | `#ffffff` | `#101720` | Working pane |
| Surface container low | `#eef2f7` | `#1a2430` | Grouping and secondary pane |
| Surface container | `#e7edf5` | `#202d3b` | Resting cards |
| Surface container high | `#dfe7f1` | `#293849` | Hover and raised grouping |
| Surface container highest | `#d7e1ed` | `#344557` | Pressed decoration |
| On surface | `#172434` | `#e8eff8` | Primary text |
| On surface variant | `#46576a` | `#b9c8da` | Supporting text |
| Outline | `#6d7e94` | `#899caf` | Field boundaries |
| Outline variant | `#c4d0df` | `#43566a` | Group separators |

Retain the six context accent roles and error/inverse roles through `MD3::resolve`, including user seed customization. New dark values must not alias the light lookup keys in `StateColor.cpp`; check that exact invariant before integration. Text contrast must be measured for actual foreground/background pairs, including state layers and user colors. Do not dark-map an already resolved color twice.

The implemented light outline is deliberately darker than the initial `#718298` proposal: against the highest container `#d7e1ed`, the initial color yields 2.969:1 and the adopted `#6d7e94` yields 3.137:1. This clears the 3:1 threshold for an essential field boundary on that background. The exact shared palette source is `b748affe0f687f6cfbe6068a82d32988047f790a`; static color arithmetic does not establish rendered accessibility. Its additional dim/bright roles are light `#d6e0ec` / `#fafcff` and dark `#111821` / `#3b4c60`. Context accents, error/inverse roles and saved user overrides remain unchanged.

### Geometry and type

| Metric, DIP | Comfortable | Compact |
| --- | ---: | ---: |
| Panel gap / padding | 12 / 16 | 7 / 10 |
| Control row / body size | 40 / 14 | 32 / 13 |
| Tool rail / inspector width | 60 / 344 | 50 / 312 |
| Card / small control radius | 16 / 10 | 12 / 8 |

Keep the established title bar at 46 DIP, navigation at 52, Prepare action region at 66 and Preview timeline at 58. Those are minimum design heights, never instructions to crop localized text. Increase content-owned height when measured text requires it. Page heading is 20 bold, section heading 16 bold, card heading 15 semibold, body 14 regular, supporting text 12.5 regular, metadata 11.5 regular. Compact body may use 13; essential text never drops below that to make a panel fit. Technical values use the existing monospaced face and explicit units. Use shipped local fonts and accessible fallback metrics, never a network font dependency.

## Component anatomy and states

| Component | Anatomy and interaction |
| --- | --- |
| Workflow navigation | 8 DIP outer horizontal inset, 6 vertical; low-container group with radius 16; destination height 40/32, icon 20, inner gap 8, horizontal padding 14. Selected destination uses primary-container/on-primary-container and a visible marker. Resting text uses on-surface-variant. |
| Buttons | Leading icon when meaningful, verb-first label, real existing callback. Filled primary for one next action; tonal secondary; outlined alternative; text tertiary. A busy action retains label and cancel route; disabled reason stays adjacent or accessible. |
| Fields | Persistent label, input/value, optional unit or browse affordance, supporting explanation/default provenance, and separate validation line. Never use a placeholder as the only label. Search owns an adjacent anchored full regex builder with isolated state. |
| Cards and panels | Header, compact description, content, optional footer actions. Opaque role surface, 1 DIP outline-variant, 16/12 radius, 16/10 padding. Scroll content inside the panel; keep headings and actions reachable. |
| Lists and tables | Clear header, local search and anchored builder, real rows, explicit selection, row-specific actions and bulk action count. Virtualize long lists. Numeric values align by column; destructive controls do not jump with telemetry. |
| Menus and overlays | Opaque surface, local search/builder, unchanged item order and enable rules, shortcut column, submenus within display bounds. Escape and dismissal return focus. Anchored first, bounded dialog where necessary. |
| Selection controls | Real registered switches, checkboxes, radio and segmented controls, never painted copies without semantics. Immediate state/value changes; decoration follows. |
| Dialogs | Title, precise explanation, meaningful content, validation and real footer actions. Destructive confirmation retains both keys, full slider and always-available cancel. Secret material never enters design examples. |
| Status and notifications | Icon plus state text, timestamp from real provenance when applicable, local progress/cancel, recovery action, and reachable history. A toast does not replace local progress or a durable error. |

Every component specifies normal, hover, focus, pressed, selected, disabled, dragged, validation, loading, success, warning and error where applicable. Resting transparency changes to high-container on hover and highest-container while pressed. Visible focus is a 2 DIP primary inset ring with 2 DIP clearance, without moving geometry. Selection also changes a marker or text. Disabled state does not remove explanation or target-specific keyboard/context routes. Do not infer support from a component name: each surface requires its own implementation and proof.

## Navigation and behavior preservation

The primary visual order is **Prepare → Preview → Print → Monitor**. Preserve the current numeric page IDs because palette and other callers address them directly. Append the new Print page instead of inserting it before existing pages. `Notebook::SetWorkflowPages(prepare, preview, print, monitor)` projects the visual order only. It must not mutate page identity, closeability, pinning, user ordering, grouping, palette results, callbacks or selection semantics. Existing Home, Project, Multi-device, Filament, Calibration, Preferences and other registered destinations remain reachable as secondary destinations and through their established routes.

Measure available width and actual localized labels. Only insufficient space moves secondary destinations to the existing overflow. Keep the selected destination visibly indicated, including when represented by overflow. Never crop labels, remove user tab controls, or replace a working action with a decorative pill. Keyboard, touch, assistive technology and command-palette paths use the same model.

Print is a real workspace, with current plate readiness, destination/mapping and a summary that continues into the existing final print confirmation. A stale slice explains why it must be regenerated. Busy and cancellation states preserve generation/plate identity. Switching to Print never slices, submits, resumes, cancels or otherwise operates hardware by itself. Existing single/multiple-printer, AMS, external spool and compatibility checks remain authoritative.

## Complete surface and state handoff

[manifest.json](workflow-refresh/manifest.json) explicitly lists every surface, its source destination, state IDs, preserved behavior, proposed composition, and static references. The independent test list keeps a deleted surface from disappearing silently. Each surface receives both light and dark structural boards. A board covers layout and hierarchy, not every state's rendered pixels. The per-state records remain unverified until a matching built route, interaction sequence and capture exist. Generic statements such as “all settings” cannot discharge the nested-control inventory.

The 43 feature families and all 1,204 obligations in `native-feature-delivery.json` remain unchanged in truth value. Language modes, funny-level controls, local vocabulary upload, School mode, narrator, schedules, tabs, search/regex, locks, history, exports, help and every other canonical obligation apply to each surface. Do not remove a function to improve a reference. Missing functions remain explicit work, not illustrative claims.

### Implementation ownership and sequence

1. Shared native chrome: implement role ramp, type hierarchy, component state layers and the four-workflow visual projection in `MD3Tokens.hpp` and `Notebook.cpp/.hpp`, retaining the existing motion and tab model.
2. Native workflow: wire the appended Print workspace, readiness snapshot and existing setup route in `MainFrame` and `WorkflowPrintPanel`, keeping numeric IDs and physical-operation semantics unchanged.
3. Prepare and Preview: apply the same card/field/toolbar system to Plater, ParamsPanel, object/plate lists, nozzle cards, preview legends and timeline. Renderer-owned canvas tools keep their actual geometry and action routes.
4. Monitor, Multi-device and Filament: apply consistent status cards, lists, fan/camera surfaces and telemetry hierarchy to native and product-owned embedded views. Network and hardware behavior stays unchanged.
5. Project, Calibration and Preferences: apply the same system to workspace files, notes/checklist/calendar, calibration sequences, all registered preference sections, parameter editors and setup wizard.
6. Menus, dialogs and auxiliary tools: complete search/regex, palette, appearance editor, notification/history/docs/changelog, import/export, model creator, schedules, confirmations and canonical tool surfaces. Retain unfinished rows until these implementations exist.

The complete incorporated-source inventory is `sourceReview` in [manifest.json](workflow-refresh/manifest.json), reconciled against `4147ca9eeb0004f7e18b4a2f5a8c6cb190975d21`. It records 69 exact source revisions across 33 families, including companion repairs, plus five documentation receipts. The six initial native scopes have incorporated source units with partial nested coverage. Product-owned embedded styling has both its 23-stylesheet paint unit and its 13-stylesheet composition unit. These source receipts do not establish full native compilation, rendered completion, geometry or runtime behavior. [implementation-scopes.json](workflow-refresh/implementation-scopes.json) and the ranked map below distinguish incorporated partial work and remaining callers. The introductory receipts below are historical milestones, not the complete current list.

| Source revision | Source receipt | Evidence limit |
| --- | --- | --- |
| `f28dad52e0a848d613f661849a4fdd64f30c3a60` | Appended Print review workspace, current-plate summary, readiness and existing action routes in `MainFrame`, `WorkflowPrintPanel` and `WorkflowPrintState` | Source only; preserve original setup/confirmation semantics; no rendered proof |
| `b69528b150ec03fd33db79f36be16c92812bdef0` | Print/navigation catalog entries and associated workspace documentation/localization check | Source only; no three-language rendering or text-fit proof |
| `624e2ed52e2fe7afcb91961fcaefe9bb201ba15e` | `Notebook::SetWorkflowPages`, measured navigation and overflow layout | Mixed presentation/API behavior; excluded from simple appearance-only reversal |
| `b748affe0f687f6cfbe6068a82d32988047f790a` | Shared native neutral palette, legacy alias compatibility and theme conversion | Appearance-only source unit with separate source/color checks; no native pixels |
| `a2be7df26cd5981da2c1e50d4c64c5e3a86609b9` | Product-owned embedded neutral colors, focus outline and reduced-motion-aware color transitions in 23 stylesheets | Paint-only; no layout, typography metrics, DOM, bridge or callback changes; no embedded pixels |
| `6faff372404390ba6cd9191c12e9fa5439ce375c` | Strengthened complete embedded interaction stylesheet assertions and explanatory article | Verification-only correction; no production CSS changes; not rendered evidence |

The shared palette reaches only consumers that actually resolve those roles. It does not prove that hard-coded colors, local component anatomy, panel geometry, fonts, every nested state or embedded CSS have changed. Integration review must compare each source path and state against the full manifest. Keep all 1,204 feature obligations at their existing evidence status until their own proof arrives.

### Whole-application composition review

The review at `be5e1205dcdad8f372d2ba63f367dbd7977c29c4` found that the original structural boards alone are insufficient to drive every detailed surface: several auxiliary boards share generic list/form layouts and do not specify all caller-owned labels, supporting text, validation, headers, footers or state changes. The 28-group/324-state inventory is necessary coverage bookkeeping, not proof that each composition is finished.

[surface-contracts.json](workflow-refresh/surface-contracts.json) now names concrete zones, component anatomy, narrow-layout rules, state transitions, preserved functions, source destinations and ownership scopes for each of the 28 groups. This is the detailed implementation contract. The existing boards remain explicitly approximate structural references, with state-specific design rendering still missing; they were not regenerated for this text-only refinement. Run `node design/workflow-refresh/check-surface-contracts.mjs` for the independent source-inventory check. Its result is not a UI or functionality verdict.

The initial ten outside-anchor groups have individual source dispositions. Workspace, calibration children/results, setup index, fields/presets, Print setup, device popups/name editor, live notifications and additional embedded composition have incorporated units, while their documented nested gaps remain. Shell/tab composition and its drag/focus repairs, plus bounded SuperConfirmGate/general MsgDialog composition and its insufficient-space correction, are incorporated in the current candidate. Connection/send continuation composition, readable-disclosure and caption-fitting repairs are also incorporated. `NotificationCenterPanel.cpp` is the actual notification reader presentation anchor; `NotificationHistory.cpp` remains its unchanged data model. Conditional existing-tool ownership, persistent field stacks, validation rows, custom control families and other caller-specific anatomy still require work after shared state-surface changes.

All work here remains a visual redesign of existing capabilities. Missing Model Creator, external-source or canonical-tool engines remain their existing incomplete feature obligations. A new decorative control, hidden function or removed workflow cannot close those obligations. No source scope may change printer, AMS, slicer, credential, export, history or scheduling semantics merely to match a drawing.

## Motion, readability and supported tuples

Use existing interruptible, weak-owner motion primitives: 100 ms input feedback, 250 ms selection wash, 300 ms panel decoration, and at most 500 ms for explanatory progress. Values, enable rules, focus, cancel and callbacks change immediately. Do not fade the OpenGL canvas, arbitrary child windows, live camera frames or essential text. No idle decorative loops. Only real activity uses continuing motion, paired with readable state. Reduced motion, hidden/minimized windows and owner destruction settle/cancel immediately and stop frame scheduling.

Normal reference size is 1200 × 800 DIP. Runtime minimum is `max(1000,76*em)` × `max(600,49*em)`, measured from the live application. Validate every surface in English, Cantonese and bilingual modes; light/dark; comfortable/compact; 100%, 125%, 150% and 200% scale; system/reduced motion. Bilingual text wraps as two measured lines or exposes secondary text through named progressive disclosure, never shrinks essential text. Lists scroll in their own pane. Dialog content scrolls independently of reachable footer actions. Toolbars wrap or use their existing overflow with every action still reachable. Sidebars retain splitter/resize behavior.

## Deterministic reference and verification boundary

Run `node design/workflow-refresh/render-references.mjs` to regenerate the checked-in SVG boards. Run it with `--check` to compare canonical LF source text, allowing the repository's normal CRLF checkout conversion. Board IDs, synthetic example labels, geometry and colors are fixed; no clock, random data, network request or font download occurs. These are explicitly labelled static design references. They must never be promoted into production screenshot evidence.

Address a reference by its manifest surface ID and theme, for example `prepare/light` resolves to `workflow-refresh/references/prepare-light.svg`. This fixed normal-size board route is not a complete reference-viewer or production fixture route. The native reference-viewer implementation, exact state/language/scale routes, runtime controls, parity comparisons and computed/native geometry receipts remain pending and are recorded as such in the manifest. Do not claim those routes exist from a filename.

After launch authorization and a real build are available, use the required isolated capture route, retaining source commit, build receipt, executable SHA-256, exact state/tuple, input sequence, accessibility names, privacy verdict and capture hashes. Native panels require measured control/text rectangles; embedded views also require the validated layout-probe receipt. Compare raw reference and built captures at identical tuples, retain labelled comparisons and differences, and repair actual clipping in that same tuple. Motion requires temporal evidence, including reversal, repeated open, hide/destruction, DPI/theme change and reduced-motion transitions. Source checks do not establish rendered contrast, text fit, accessibility, function preservation, or printer success.

## Remaining visual implementation coverage

Reviewed source candidate: `4147ca9eeb0004f7e18b4a2f5a8c6cb190975d21`.

The source ledger records 69 exact units across 33 families, plus five documentation
receipts: seven appearance-only, twenty-seven appearance/layout, twenty-seven correctness
repairs, two functional-workflow, four verification-only and two localization units.
The formerly pending shell unit is now incorporated with its drag/focus repairs.
Numeric input allocation, calibration viewport/preset measurement, humidity final
placement and Appearance preset reflow are recorded as separate repairs. Transform
field paint is appearance-only. No integration merge is a reversal handle, and no
repair becomes disposable merely because its parent unit changes appearance.

The later snapshot adds the StaticBox radius compiler correction, calibration
result viewport and vertical-wheel ownership plus the default fixture correction,
device-name layout and validation-fitting repair, common framing for twelve tool
inspectors, Regex/Export detail composition, and the shell/tab Cantonese companion.
Connection/send continuation unit `b60bd4c8b8fa5eb0d3e5a9f706621e73f8a1a476` is now
incorporated with readable-disclosure repair `8b8d042e`, caption fitting `ee4fc3ec`
and fixture-only address correction `59405fc8`. The prior hold is retired by
verified source ancestry, not by promoting runtime or functional-delivery evidence.
The latest snapshot adds kit scroll/table-owner adoption `334eb7fa` across six
Schedule, Print, Workspace and bulk caller sites, plus catalog reconciliation
`7a681fd0f`. The former is appearance/layout integration, the latter localization;
neither is a pure-paint reversal or evidence that a functional obligation is complete.
This documentation-only reconciliation assigns no new implementation scope.

The nine rows below are the finite remaining whole-look review boundaries from the existing contracts and implementation destinations. They identify source coverage or native-proof gaps, not nine new implementation assignments. Built review must establish a concrete gap before further UI edits. Every runtime, capture and parity verdict remains pending. A named source unit means source is incorporated, not that every nested control is finished. Functional gaps are recorded separately below.

| Rank | Remaining visual coverage boundary | Existing source and built-review boundary |
| --- | --- | --- |
| 1 | Shared controls and caller anatomy used throughout the application | `SpinInput`, `CheckBox` and `SwitchButton` now have bounded source anatomy, including native numeric allocation repair. Their actual narrow callers still need validation. ListBox now has measured rows, focus paint and displaced-hover invalidation. Slider tracks, `SideMenuPopup`, `TextTabbar`, `LabeledCheckBox`, `ImageSwitchButton` and other toggle classes remain independently owned. Option-name/default/reset/inherited markers, persistent inline validation and full narrow field stacks remain caller-owned. Shared button tiers remain 36/42/44 DIP; the proposed 40/32 density contract has not universally landed. |
| 2 | Shell, project tab strip, nested books and tab dialogs | Shell/title/history/TabStrip/Tabbook source and visible-neighbor drag plus focus/accessibility repairs are incorporated. Overflow remains layout behavior, not paint-only. Full wrapped vertical labels, extreme dialog fit, title-toolbar keyboard evidence and unrelated status owners remain separate. |
| 3 | Real final-action and recovery continuations | Print review/setup, bounded SuperConfirmGate/general MsgDialog composition and the device-name editor are incorporated. Connection/send confirmations, IP entry and send-failure presentation plus disclosure/caption repairs are incorporated. Material/nozzle tips/tutorial/replacement/reselection, final send variants, local timelapse/storage warnings, account/bind/PIN dialogs, other transfer-failure prompts and specialized text/number/choice message branches remain distinct surfaces. Preserve every authorization and physical-action path. |
| 4 | Dense settings, calibration and schedule subforms | Calibration instruction/advice measurement, whole preset-row wrapping, stale-height release, intrinsic extents, result-table horizontal viewports and outer vertical-wheel ownership are incorporated. Specialized settings/reset dialogs, schedule rule-editor conditional fields and further capability-dependent result controls remain. Native font, wheel/focus and scroll behavior is unverified even where source composition exists. |
| 5 | Native printer-specific detail and telemetry composition | Humidity legend/value composition and final popup placement are incorporated alongside the Control-header DPI repair. Nozzle rack/selection/hotend rows and AMS drying cards/pages/error disclosure are also incorporated. Mapping wheel/multicolor detail, material search, pressure-advance profile popovers, custom-color editing, AMS type/order, nested device dialogs and further capability-dependent rows remain independently owned. |
| 6 | Nested readers/editors and live notice variants | Appearance groups/preset reflow, Regex diagnostic/sample/result composition and Export fidelity/tool-path disclosure rows are incorporated. Font-family search/preview, palette rich rows, Regex reference tables/chips/flags, history filters, documentation article controls, changelog day grid/filter/export, Export checkboxes/destination controls and other detail branches remain individually owned. Live notification blocking banners and semantic state/icon inventory remain unchanged. |
| 7 | Workspace's inherited detail surfaces | Five workspace subviews and stable-width Overview reflow are incorporated. Existing text-entry/file dialogs, checklist long-label shortening, tab overflow/keyboard and any missing local search/regex remain separate. No History subview exists in `WorkspacePanel`; history-bearing member save remains intact. Do not introduce a new history engine merely to fill the design board. |
| 8 | Embedded entrypoint and alternate-flow composition | Both the 23-stylesheet paint unit and the 13-stylesheet composition unit are incorporated. The home mini-strip item-count policy, alternate filament-creation flows, JavaScript motion and the pre-existing setup-guide test mismatch remain. Trace actual native-host entrypoints and owned markup before any further CSS scope; preserve event targets, bridge protocols and third-party assets. |
| 9 | Renderer detail beyond recorded paint | Toolbar/rail, overlays, Preview transport/legend, transform fields and the common floating-inspector frame for twelve paired callers are incorporated. Transform work preserves fifteen numeric inputs and their geometry, axis identity and data colors. Shared framing does not redesign each tool's interior controls. `GLCanvas3D.cpp`, tool interiors, canvas-specific panels and remaining tooltip layout still need individual ownership inventory. |

The kit-owner adoption now covers the previously raw Schedule editor/panel, Print
review, Workspace section-page factory and both bulk preview tables. Their models,
constructor arguments and callbacks are preserved in the source receipt. This does
not establish native scroll behavior or finish all nested control composition.

### Disposition of the three previously prioritized families

At historical candidate `c7868b48536e8f8277d6872bd7d8def9d8463d02`, these owner
files were unchanged from the original design baseline. At the current candidate,
all three have incorporated source units. They are no longer described as untouched
or proposed next work. The original reachable owner paths remain in the scope map.

| Family | Incorporated source disposition | Remaining proof boundary |
| --- | --- | --- |
| Shared ListBox rows | `ed3cbc35` adds measured row/check anatomy and focus/hover paint; `c8b269d6` invalidates displaced fading rows. | Real caller minima, large text, narrow glyphs, keyboard and paint delivery remain unverified. |
| AMS drying pages | `fffdc0a9` adds density-aware cards, grouped controls and scrolling; `2b6c799c` attaches the existing error disclosure to its visible state owner. | Native scroll/focus, content updates, language/theme/scale tuples and device interaction remain unverified. |
| Nozzle rack details | `d31530a3` adds measured rack/selection cards and hotend rows, retaining mapping and hardware identities. | Outer owner allocation, long values, popup fit, native selection and supported display tuples remain unverified. |

`implementation-scopes.json.followupSurfaceFamilies` records the historical
unchanged-source observation separately from current incorporation. Its checker
verifies both facts and the real call sites. No new source implementation is assigned
by this documentation unit; the candidate is ready for the integration owner's
separate pinned native build process.

### Separate functional-feature gaps

The separate functional inventory still contains all 1,204 obligations with their
existing evidence states. Missing, incomplete, planned and placeholder functions
stay that way. Reachable controls, a new visual plan, a source receipt or an index
link cannot promote a functional obligation. The broader feature objective remains
unfinished wherever that ledger says so; these three visual scopes neither erase
those gaps nor silently add new feature engines.

### Acceptance common to every rank

All 56 checked-in SVG boards remain structural illustrations, not screenshots. They do not supply every state-specific reference, native reference-viewer route or real-product fixture. Native compilation and the genuine normal/minimum size, language, theme, density, display-scale, keyboard, focus, reduced-motion and temporal matrix remain separate work. Existing source checks and mathematical fixtures do not replace those results.

The nine visual boundaries have exact existing destinations in `implementation-scopes.json.remainingVisualCoverage`. They remain awaiting built review, with `implementationAssigned: false`. Missing Model Creator, external-source and canonical-tool capabilities remain functional backlog in the original ledger, not additional visual assignments. No row authorizes changes to feature engines, printer commands, credentials, data persistence, callbacks or unrelated fixes.
