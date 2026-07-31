# Notifications

Implemented in [`ui-md3/site/core.js`](../../../ui-md3/site/core.js); the centre is wired up in
[`ui-md3/site/boot.js`](../../../ui-md3/site/boot.js).

## Behaviour

Informational, success, progress and non-decision error messages appear as toasts anchored in the
bottom-right corner. They stack vertically without overlapping, carry an icon, a message, an
optional action, and a 44px dismiss target.

| Kind | Auto-dismiss | Role |
|:---|:---|:---|
| `info` | 6 s | `status` |
| `success` | 5 s | `status` |
| `warning` | never — dismissed by the user | `alert` |
| `error` | never — dismissed by the user | `alert` |

**The host carries no live region.** Each toast carries its own role — `status` for info and
success, `alert` for warnings and errors — and `applyCopy` populates it *before* it is attached, so
an alert enters the accessibility tree already carrying its text. An `aria-live` host wrapping
per-toast live regions is a nested live region: assistive technology announces twice, and the
polite host softens the urgency of the error inside it.

## What is never a toast

A decision the user must make before anything happens is a modal dialog, and there is exactly one
on the site: [resetting all stored settings](settings-and-appearance.md#the-one-blocking-dialog).
Everything else — a setting saved, a pattern copied, an export written, a tab pinned, the clipboard
refused, local storage blocked — is a notification.

## Notification centre

The bell in the header opens a history of the last 50 notifications with their times, so a toast
that auto-dismissed is still reviewable. A badge shows the count. "Clear all" empties both the
history and any toast still on screen.

## The disable switch, and its limits

"Show notifications" turns off the toasts for informational and success messages. Warnings and
errors are still shown, because a message the user needs in order to act is not decoration — and
every kind, shown or not, is still recorded in the centre. The setting's own description says this
plainly.

## Localisation

A notification stores its copy **key and parameters**, not a rendered string. Changing the language
mode or a funny level re-renders the ones already in the centre, and the parameters — the setting
name, the export count, the engine's error message — are interpolated at render time, so no
notification can lose its facts to a tone change.
