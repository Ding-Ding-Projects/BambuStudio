# Studio Atlas live notification presentation

This appearance-only unit applies the notification composition contract in design/workflow-refresh/surface-contracts.json to the existing renderer-owned cards. It does not change NotificationCenterPanel or notification history. The source baseline is 35a329da565a61b98f541166e51b85c44958add8.

## Composition and scope

The inverse-surface card and its existing measured body type remain the foundation. The large full-height colored left slab becomes a slim inset status rail, bounded by the existing text indentation. The rail starts below the rounded corner, and a subtle inverse-content outline defines the card without introducing a layout border. The existing warning, error and ordinary accent selection remains authoritative.

The card radius reads the existing comfortable/compact density and current canvas scale while the notice is alive. Visible close, minimize and upload-cancel controls receive translucent inverse-content hover and pressed layers. Their enlarged invisible interaction targets remain transparent so their feedback cannot paint across text or the card corner. No label, icon, button position, hit rectangle, font size, width or wrapping rule changes.

The progress track uses translucent inverse content, paired with the inverse card, while its existing accent fill, endpoints, percentage and text retain their original computation. Fading uses the existing opacity value. This change adds no animation, scheduling or frame requests.

Exactly these functions in NotificationManager.cpp are changed:

- PopNotification::ensure_ui_inited: refresh decorative radius from density and canvas scale.
- PopNotification::bbl_render_left_sign: draw the bounded rail and inset outline.
- PopNotification::render_close_button: visible state layers, transparent enlarged target.
- PopNotification::render_minimize_button: visible state layers.
- PrintHostUploadNotification::render_cancel_button: visible state layers, transparent enlarged target.
- ProgressBarNotification::render_bar: inverse-paired track color and alpha only.

The saturated blocking warning/error banner path is unchanged. Existing icon availability and message semantics remain unchanged; this unit does not claim new state text, a new semantic status icon inventory, or complete notification feature delivery.

## Preservation and verification

The following existing source checks passed, eight total:

~~~text
node --test ui-md3/tests/notification-bilingual-links.test.mjs ui-md3/tests/preview-overlays.test.mjs
~~~

The bilingual-link check was observed rejecting a deliberate replacement of the measured bilingual action label with its raw label. The original bytes were restored and the full focused command passed again. No new paint-value-only test was added.

A source comparison against the baseline confirmed eleven function bodies unchanged after line-ending normalization: PopNotification::render, bbl_render_block_notification, fit_to_stack, count_spaces, count_lines, set_next_window_size, render_text, on_text_click, on_second_text_click, update_state, and NotificationManager::render_notifications. These preserve stack and dock coordination, text measurement, lifecycle, timeout handling and action dispatch. The three changed action-paint functions retain their original target calculations and callback statements.

No full build, application launch, screenshot, hardware operation, installer, release or deployment was performed. Native compilation and real-card inspection are still required, including all supported languages, themes, density and display scales; hover/pressed and disabled behavior; scrolling; overlapping notifications; reduced motion; user-seed contrast; progress and cancellation. Source checks cannot prove those runtime results.

## Reversal boundary

This unit contains only NotificationManager.cpp presentation edits and this article, separate from the earlier renderer and tooltip revisions. Revert this unit independently if the appearance is rejected. Preserve the baseline renderer fixes and unrelated notification history, lifecycle, transport, cancellation and layout work. Do not reset the repository or remove existing notification functionality to reverse this appearance.
