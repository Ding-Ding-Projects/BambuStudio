# Non-blocking notifications

Informational, success, warning, and error messages that require no decision
are presented as non-blocking corner toasts (the existing in-canvas
`NotificationManager` snackbars), not modal dialogs. Modal dialogs remain
reserved for genuine decisions: confirmations, unsaved-changes prompts,
destructive-action gates, and credential steps.

## Behavior

The three central message funnels in `src/slic3r/GUI/GUI.cpp` route to corner
toasts:

| Funnel | Level | Toast behavior |
| --- | --- | --- |
| `show_info(parent, message, title)` | Regular | fades out (~10 s) |
| `warning_catcher(parent, message)` | Warning | persists until dismissed |
| `show_error(parent, message)` | Error | persists until dismissed; topmost |

This covers ~120 OK-only call sites app-wide without touching each caller.
Toasts render on the GL canvas (bottom-right corner region), stack without
overlapping, and support the manager's hyperlink/action affordances.

### Modal fallback

`try_push_corner_notification` falls back to the original modal dialog when a
toast could not be seen:

- before the Plater / NotificationManager exist (early startup, e.g. config
  wizard failures), or
- while another modal dialog is on top (the toast canvas would be covered —
  detected by scanning `wxTopLevelWindows` for an active `wxDialog::IsModal()`).

No message is ever silently dropped: the fallback shows the exact dialog the
call site used before this feature.

## Configuration

None. The routing is unconditional; the notification history remains reviewable
through the notification manager's stacked list on the canvas.

## Failure modes

- **Modal on top** → modal fallback (by design, see above).
- **Message posted from a worker thread**: `show_error` already marshals via
  `CallAfter`; `show_info`/`warning_catcher` retain their original thread
  expectations (main thread), unchanged from before.
- **GL canvas unavailable** (headless/init failure) → the Plater check fails →
  modal fallback.

## Security considerations

Message text is rendered by ImGui as plain text; hypertext actions are only
those explicitly wired by call sites. No message content is interpreted as
markup or executed.

## Verification

- Built clean into `libslic3r_gui` (0 errors) and shipped in commit
  `ab007cda2`.
- Headless visual proof: captured in the screenshot matrix under
  `docs/screenshots/notifications/` (toast render on the Mesa GL canvas via
  PrintWindow).
