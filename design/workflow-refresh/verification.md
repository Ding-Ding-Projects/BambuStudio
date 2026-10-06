# Source specification verification

This record concerns the design inventory and static source boards only. No application, installer, reference viewer or printer was launched. No production capture, hardware operation or publication was performed.

## Inventory

- 28 explicitly named surface groups.
- 324 explicitly listed workflow states across those groups.
- 56 checked-in SVG structural boards, one light and one dark board per group.
- The original 43 feature families and 1,204 per-surface obligations retain their prior evidence status.
- Complete per-element and nested-state implementation coverage remains unresolved. State counts are inventory, not implementation counts.

## Executed checks

| Command | Observed result |
| --- | --- |
| `node design/workflow-refresh/render-references.mjs --check` | 56 deterministic reference sources matched after canonical LF normalization |
| `node --test ui-md3/tests/native-feature-delivery.test.mjs` | 9 passed, 0 failed |
| Same test command with `NATIVE_DESIGN_REMOVE_SURFACE=1` | 8 passed, 1 failed, exit 1; `missing design surface` for `canonical-tools` |
| Same test command after removing that environment override | 9 passed, 0 failed |

The negative run mutates only an in-memory test fixture; it does not delete a source file. The tests also reject a missing final-confirmation state, absent themed reference, changed numeric-page identity contract and an unsupported promotion to runtime/parity verification. These are source-inventory checks, not a claim of full product enforcement.

## Static color arithmetic

WCAG sRGB relative-luminance arithmetic for the specified default colors produced these ratios. This does not test rasterized text, custom seeds, native double-remapping, intermediate animation states or the actual display.

| Pair | Light | Dark |
| --- | ---: | ---: |
| On surface / surface | 14.88:1 | 14.81:1 |
| Supporting text / surface | 7.03:1 | 10.07:1 |
| On surface / lowest container | 15.69:1 | 15.56:1 |
| Supporting text / low container | 6.60:1 | 9.21:1 |
| On primary / primary | 6.53:1 | 7.75:1 |
| On primary container / primary container | 13.29:1 | 7.29:1 |

The initial light field outline `#718298` against the highest container `#d7e1ed` measured 2.969:1. The reviewed palette source changes it to `#6d7e94`, measuring 3.137:1 on the same background. The shared source references now use the implemented color. The dark outline is unchanged. These ratios describe default source colors, not live pixels or custom appearance values.

## First implementation receipts

The manifest records exact source revisions for the Print workspace (`f28dad52e0a848d613f661849a4fdd64f30c3a60`), localization (`b69528b150ec03fd33db79f36be16c92812bdef0`), navigation (`624e2ed52e2fe7afcb91961fcaefe9bb201ba15e`) and shared palette (`b748affe0f687f6cfbe6068a82d32988047f790a`). Their relevant paths and source were inspected to establish the handoff references. Their own reported checks are not rerun or upgraded here. None is a native-build, interaction, capture or visual-parity receipt. The palette is separately reversible; navigation owns API/overflow behavior and is not a simple appearance-only reversal candidate.

## Remaining proof

Runtime implementation for the full surface set, no-function-loss interaction coverage, exact state fixtures, reference-viewer implementation, native text/control geometry, embedded-view layout probes, actual font metrics, keyboard/screen-reader behavior, temporal motion, localization, all supported size/scale tuples, and paired visual differences remain pending. Fonts are locally resolved by a future viewer, so byte-deterministic SVG generation alone does not establish pixel-deterministic rendering. The source boards must not be filed as production screenshot evidence.

## Composition review at the combined source candidate

The subsequent review started from `be5e1205dcdad8f372d2ba63f367dbd7977c29c4` in an isolated checkout. It adds `surface-contracts.json` with 28 independent surface contracts, each naming zones, anatomy, narrow-layout behavior, state changes, preserved functions, exact source anchors and ownership scopes. The implementation map records six active native scopes and ten outside-anchor followups. These counts describe assigned work and specification coverage, not completed components.

`node design/workflow-refresh/check-surface-contracts.mjs` passed. With `NATIVE_DESIGN_REMOVE_ANATOMY=1`, the checker deliberately removed the confirmation surface from an in-memory fixture and exited 1 with `Missing explicit surface anatomy`. Removing the override restored a passing result. The existing `ui-md3/tests/native-feature-delivery.test.mjs` suite passed 9 of 9 cases against the refined manifest. All referenced source paths exist at the reviewed baseline.

The original feature ledger is byte-for-byte unchanged relative to that baseline, including all 1,204 obligation truth states. The 56 structural boards are also unchanged: this refinement changes the detailed textual contract and ownership/evidence metadata, not their renderer input geometry or colors. The generic boards remain incomplete as exact state-specific design references, and no regeneration or new visual acceptance is claimed.

Source receipts `a2be7df26cd5981da2c1e50d4c64c5e3a86609b9` and `6faff372404390ba6cd9191c12e9fa5439ce375c` are recorded separately. The first changes appearance in 23 embedded stylesheets while preserving geometry, typography metrics and callbacks; the second corrects source verification and its article without changing production CSS. Neither establishes embedded composition or runtime rendering. Reversal review distinguishes those units from functional navigation and Print work.

## Incorporated-source reconciliation at f3565c758

The design inventory was safely fast-forwarded to reviewed candidate `f3565c7580bebd38bcbf5c5f01e272fa5c4b6224`. `manifest.json.sourceReview` and the reversal ledger now agree on 31 exact incorporated revisions across 18 families: five appearance-only, twelve appearance/layout, nine correctness repairs, two functional workflow, two verification-only and one localization unit. Each revision's local ancestry into that exact candidate was checked, as was the existence of its supporting article. The reported shell/tab revision `44cba370a948dc6653189e8c835274d2db10d0ac` was checked separately and is not an ancestor of the reviewed candidate.

The updated source checker passed. Setting `NATIVE_DESIGN_REMOVE_RECEIPT=1` removes the workspace receipts from an in-memory fixture and produced exit 1 with `Missing incorporated source receipt`; clearing it restored a passing result. The existing feature/design ledger suite passed 9 of 9 cases. These checks protect receipt completeness, exact local ancestry and the boundary preventing a mixed layout/repair unit from becoming paint-only. They do not rerun or upgrade the implementation units' own tests.

The 1,204 feature obligations and all 56 structural boards remain unchanged. No reference regeneration, production build, launch, capture or rendered acceptance occurred. `workflow-refresh.md` now ranks concrete remaining caller/subview implementation work by coverage instead of repeating the universal feature audit.

## Incorporated-source reconciliation at 432eb0f7f

The isolated documentation checkout fast-forwarded to immutable candidate
`432eb0f7f2147de49479654d0f94f1929f192fa2`. The manifest and reversal ledger now
agree on 48 exact source units across 24 families: six appearance-only, nineteen
appearance/layout, eighteen correctness repairs, two functional-workflow, two
verification-only and one localization unit. Four separate documentation receipts
are ancestry-checked without being counted as additional production surface work.

The 17 added units register shell/tab layout and drag/focus repairs, confirmation
composition and insufficient-space cancellation, Control-heading DPI repair,
selection controls and numeric allocation repair, calibration viewport/preset
layout and height/intrinsic-width corrections, transform paint, humidity layout
and final placement repair, and Appearance property layout/content reflow.
Integration merges, build/bootstrap/OCCT repairs and unrelated AMS behavior are
not appearance rollback handles. No automatic rollback is permitted.

The source receipt checker passes, including ancestry, reversal classifications,
candidate agreement and all supporting article links in both indexes. Removing
the numeric-selection article link from its in-memory index produces exit 1 with
`Missing incorporated article link`; removing Workspace receipts produces exit 1
with `Missing incorporated source receipt`. The nine feature/design tests pass,
and the deterministic-reference check still matches all 56 boards. The Cantonese
index hash binds its current English source. Calibration viewport Cantonese stays
inline in the real article; no separate companion is invented. Shell/tab and
native-workflow navigation retain their currently English-only source links.

The 1,204 obligations and 56 board files are byte-unchanged from this candidate.
All runtime, reference-viewer, fixture, capture and parity truth states are
preserved. No production source, full build, launch or publication is part of
this reconciliation. Remaining caller, nested-dialog, calibration-result,
device-detail, reader and renderer gaps remain explicitly open.

## Incorporated-source reconciliation at c7868b485

The owned checkout fast-forwarded to immutable candidate
`c7868b48536e8f8277d6872bd7d8def9d8463d02`. The records now contain 56 source units
across 27 families and five documentation receipts. Added units cover the runtime
density-radius compiler correction, calibration result viewports and vertical
wheel ownership with the default fixture correction, device-name presentation and
validation fitting, common framing for twelve floating inspectors, Regex/Export
details and the shell/tab Cantonese companion. The eight new source receipts are
one appearance-only, three appearance/layout, three correctness-repair and one
verification-only unit. No existing reversal class was relaxed.

Connection/send unit `b60bd4c8b8fa5eb0d3e5a9f706621e73f8a1a476` is explicitly held
outside this candidate for disclosure-availability repair. Its absence is checked
with local ancestry. No completion or article-index inclusion is derived from it.
The current composition paragraphs and ranked coverage table agree with this
snapshot; earlier reconciliation sections in this verification file are historical.

The updated receipt checker verifies ancestry, exact candidate agreement, both
article indexes and their translation hash, plus three concrete untouched source
families: ListBox rows, AMS drying pages and nozzle rack/selection/update details.
For each, it verifies unchanged owner files since the original design baseline
and actual reachable call-site text. This is source reachability evidence only.
It neither executes a route nor implements the proposed visual follow-up.

The minimum changed-ledger/index check passed. An in-memory stale-composition
mutation (`NATIVE_DESIGN_STALE_COMPOSITION=1`) is rejected with the assertion that
the composition summary still excludes incorporated shell work; the unmodified
summary passes. The checker also preserves the explicit incomplete-engine sentence.

All 1,204 obligation rows and all 56 structural references remain byte-unchanged.
Runtime, captures, fixtures, reference viewer and parity remain unverified/missing
as previously recorded. Missing, planned, placeholder and incomplete functional
obligations are not erased or advanced by this visual inventory. No UI source,
full build, launch, capture or publication occurred during reconciliation.
