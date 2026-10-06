# Studio Atlas confirmations and general message dialogs

## Source-only scope

This unit implements the confirmation/general-dialog appearance contract from `3c8fe2708ba03470c1b171e2fce12fcbdfaab0fc`. It starts from `71170bc334656d51af49d3ae682ae14320a110f5` in an isolated checkout. Only `Widgets/SuperConfirmGate.cpp` and `MsgDialog.cpp` change in production. No header, authorization condition, independent key semantics, slider threshold, default action, callback dispatch, or affected-data text is changed.

No application launch, full native build, screenshot, installer execution or hardware operation occurred. Source checks cannot establish native compilation, text fit, contrast, keyboard behavior or runtime accessibility.

## Composition

- Super confirmation has a neutral shell, a visible consequence heading and inset low-container affected-name detail, distinct stage heading, two independently labelled keys, unchanged progress/slider and a separated stable emergency-cancel footer.
- Only the affected-name detail enters the kit scroll pane. The consequence, total affected count, both keys, charge, slider and emergency exit remain outside it. Detail text is measured within its inset width, with space reserved for the scrollbar. The shell caps its initial size to the display work area and lets only the detail viewport shrink. Existing anchor placement remains authoritative.
- General message bodies receive density-based inset reading surfaces for both plain and HTML branches. Text width reserves the inset and scrollbar before measurement; viewport width restores that allowance. Existing bilingual assembly occurs before measurement.
- Footer measurement and actual grid gaps use the same active density value, including a later reflow. Button order, labels, default focus and results remain unchanged.
- Note dialogs use semantic primary/supporting text and the existing bounded-width helper. Supporting notes use the readable body-13 face instead of body-12. The main shell still owns message-dialog action separation and work-area fitting.

## Behavioral preservation evidence

`ui-md3/tests/confirmation-atlas-preservation.json` records normalized-source SHA-256 fingerprints from the baseline. `confirmation-atlas.test.mjs` extracts balanced method bodies while masking comments and literals for brace counting. It compares actual implementations, not just callback registration names:

- Nine confirmation methods: blocking and asynchronous entry, stage text, stage refresh, key input, progress input, completion, terminal dispatch and focus return.
- Five message methods: style/default selection, button creation/dispatch, button label changes, button addition and checkbox-state readback.
- Complete key-switch input/accessibility class, `SuperConfirmState.hpp`, and `SlideToConfirm.cpp`.
- In-memory negative mutations remove the final authorization condition, let progress reach completion, and make both inputs operate key zero. Each must be rejected by the matching behavior fingerprint.
- Source layout checks require affected detail inside the scrolling pane and authorization/cancel controls outside it. Message inset and footer geometry checks verify matched measurement constants.

Run:

```sh
node --test ui-md3/tests/confirmation-atlas.test.mjs ui-md3/tests/message-dialog-bilingual-body.test.mjs
```

Result: **22 checks passed**. The existing bilingual suite contributes two checks. The fingerprints are intentionally strict preservation evidence for this appearance unit; a later legitimate behavioral change requires independent semantic review before updating them. They are not a substitute for executing the state-machine tests or compiling native sources. No new translation keys were introduced. `git diff --check` passed.

## Remaining runtime work

Verify untouched, one-key, both-key, partial-slide, authorized and canceled states, including keyboard completion, lost activation, close, escape, callback exactly-once behavior and focus return. Verify long affected names, eight-name truncation with total count, empty affected lists, long consequence text, bilingual disclosure and all stage strings. At minimum/normal displays and 100/125/150/200% scale, confirm the detail can scroll while both keys, full slider and emergency exit remain reachable. Extremely small work areas below the combined fixed-control minimum are not claimed supported by source inspection.

For general dialogs, inspect plain, bilingual, marked HTML, table, monospaced, link and note content, all default-action combinations, long translated buttons, do-not-show-again text, narrow action stacking and cross-monitor reflow. The existing message family also includes text/number/multiple-choice entry, post-process script review, delete confirmation, newer-project-version notice, network explanation and filament warning subclasses. This unit does not claim those nested layouts are fully refreshed or proven merely because their shared reading path changed. Device/network dispatch and destructive conditions stay owned by their original callers.
