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
