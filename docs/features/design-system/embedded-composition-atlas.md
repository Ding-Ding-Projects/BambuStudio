# Studio Atlas embedded composition

This source-only CSS unit refines the product-owned embedded composition assigned by design/workflow-refresh/surface-contracts.json at 3c8fe270. Its baseline is 8cfce63ae05d7be03823b3eb9e4b86c4488245b1. It does not alter JavaScript, HTML, bridge messages, authentication, networking, third-party styles or a standalone website.

## Entrypoint coverage

Fourteen stylesheet imports are covered across these nine existing HTML entrypoints:

| Entrypoint below resources/web | Owned styles | Composition |
| --- | --- | --- |
| homepage3/home.html | homepage3/css/home.css, recent.css, online.css, manual.css | Responsive full recent-file grid, separated section headers, readable card names and metadata, wrapping status rows and guide descriptions |
| homepage3/left.html | homepage3/css/left.css | Grouped account/navigation spacing, wrapping destination text and explicit selected marker above the existing mini-rail breakpoint |
| homepage3/wiki.html | homepage3/css/wiki.css | Grouped search/tabs, readable topic cards without a fixed 600px minimum, scrollable search results and narrower outer spacing |
| model_new/index.html | model_new/css/gallery.css, navigation.css | Framed gallery group, consistent thumbnail selection bounds, narrow vertical thumbnail stack, tonal navigation sections and focus treatment |
| model_new/editor.html | model_new/css/tool.css, accessory_dropdown.css | Message/action alignment in existing toasts, bounded accessory-list scrolling and aligned label/value metadata |
| filament_create/step2.html | filament_create/step2.css | Contained parameter workspace, wrapped tab/category groups, readable parameter rows, scrollable content and reachable footer |
| guide/1/index.html | guide/1/1.css | Content-sized welcome heading and explanation with a separate primary-action region |
| guide/23/index.html | guide/23/23.css | Filter groups, wrapping category labels, selected tabs and framed custom-filament rows retaining actual edit/delete actions |
| filament_create/edit_filament.html | guide/23/23.css | Additional direct consumer of the shared filament-list styles; edit dialog and bridge semantics remain unchanged |

The alternate filament_create/step2_type.html and step2_copy.html entrypoints import different styles and are not changed by this unit. Do not count them as covered merely because their names include step2.

## Behavior boundaries

The existing original CSS declarations remain intact, followed by scoped composition additions. Hidden route sections, inactive dialogs, login/plugin states, file masks, selection checkboxes, accessory inputs, disabled nozzle states and hidden custom groups retain their existing display rules. No new opacity, visibility, pointer-events, display:none or important override is introduced.

The home mini strips retain the original item-count policy. Only the full recent-file collection changes to a grid. Wiki carousel item widths and script-computed scrolling remain unchanged. Gallery JavaScript retains its image selection, sizing inputs and keyboard behavior; navigation retains measured indicator positions and anchor dispatch. The accessory dropdown retains its original anchor and inline display ownership, with bounded internal scrolling. The filament parameter view retains IDs, selectors and active/disabled state handling. No data-derived swatch, thumbnail, percentage or status value is rewritten.

Composition uses the existing theme variables and local font routes. Additional reduced-motion rules settle the affected CSS transitions. Existing JavaScript-driven animations are untouched; this unit does not claim to make those animations respect reduced motion.

## Source evidence

- Initially verified thirteen stylesheet imports across eight HTML entrypoints. Follow-up inspection identified the additional direct import of guide/23/23.css from filament_create/edit_filament.html, giving fourteen imports across nine entrypoints.
- Verified that all thirteen changed files preserve their baseline declarations and that additions contain balanced blocks and no hiding/opacity/pointer-event or important overrides. This is a source-boundary inspection, not a full CSS parser or browser-layout result.
- Ran node --test tests/web_resources_accessibility.test.mjs: three checks passed and one failed. The passing checks cover project zoom/keyboard preservation, native project actions and focus styles, and toast severity/live roles/dismissal/persistence/stacking.
- The failing setup-guide check expects an inline CFEdit button handler in guide/23/23.js. That file is byte-identical to the baseline after line-ending normalization and uses a bound click handler calling CFEdit(id). The same inline-handler expectation already mismatches the baseline. No production JavaScript or test expectation was changed to conceal it.
- No full build, application/browser launch, screenshot, deployment or hardware operation was performed.

## Remaining acceptance and reversal

Actual entrypoint reachability through the native host, light/dark rendering, bilingual text fit, compact settings, supported display scales, keyboard focus, carousel scrolling, image selection, dialog bounds and cancel/recovery actions still require real embedded runtime verification. No rendered acceptance, complete redesign or complete accessibility claim follows from the source checks.

The inherited home mini-strip item-count policy, alternate filament creation flows, JavaScript motion behavior and the pre-existing setup-guide test mismatch remain explicit follow-up boundaries. Revert this CSS-only unit independently if its appearance is rejected, preserving the prior renderer, tooltip, notification and unrelated functional work.

## Thumbnail reveal-axis correction

The first narrow gallery rule created a horizontal thumbnail strip, but the unchanged setActiveThumb handler reveals the active thumbnail using position().top, outerHeight(), height() and scrollTop(). The corrected narrow rule retains a vertical block stack inside the existing max-height scroll container. No gallery JavaScript changed.

The focused tests/web_gallery_reveal_axis.test.mjs executes the actual reveal function with bounded test doubles for above/below-viewport selections, then checks the thumbnail CSS declarations for incompatible horizontal overflow or row layout. The original CSS produced one passing handler check and one failing axis check. After correction both checks pass. This is source and handler-model evidence, not a browser-layout or keyboard-runtime verdict. The inherited accessibility suite result remains three passed and one baseline inline-handler expectation failure.
