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

## Remaining proof

Runtime implementation for the full surface set, no-function-loss interaction coverage, exact state fixtures, reference-viewer implementation, native text/control geometry, embedded-view layout probes, actual font metrics, keyboard/screen-reader behavior, temporal motion, localization, all supported size/scale tuples, and paired visual differences remain pending. Fonts are locally resolved by a future viewer, so byte-deterministic SVG generation alone does not establish pixel-deterministic rendering. The source boards must not be filed as production screenshot evidence.
