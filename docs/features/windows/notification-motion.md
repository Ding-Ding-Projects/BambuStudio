# Notification and progress motion

This slice connects existing notification fades and native `ProgressBar` pulses
to the shared `MD3::Motion::reduced()` preference. It adds no setting or translated
copy. Both the saved application preference and the operating system's request
are resolved by the existing shared policy.

## Notification lifetime

Notifications retain their existing readable lifetime, hover pause and overflow
deferral. Once an unpaused notification enters its decorative fade, reduced
motion moves it directly to the finished state. Normal motion retains the
existing bounded fade. Persistent notifications remain persistent, and explicit
dismissal and cancellation callbacks do not wait for a transition.

The manager schedules future notification frames only while its actual canvas
exists, is not being deleted and is shown on screen. This does not cancel
semantic updates or change progress values. A frame already requested elsewhere
can still arrive; this check prevents the notification scheduler from sustaining
an animation loop on a hidden surface. Existing inactive-window handling remains
unchanged. Notification position and button geometry do not animate.

## Native progress bars

Determinate `SetValue`/`SetProgress` remain immediate. No interpolation changes a
reported value, delays completion or invents progress. `Pulse` means activity
with no known percentage. Under reduced motion it displays a stationary centered
segment, without changing the stored numeric value or claiming completion.
The segment also remains visible if a native timer cannot start.

The pulse timer runs only for an enabled, visible, indeterminate control while
motion is permitted. `Disable`, a determinate update, an explicit hide and
destruction stop it. If an ancestor hides the control, a previously queued tick
observes `IsShownOnScreen()` and stops, at the next delivered timer event. A live
change to reduced motion is observed the same way, normally within the existing
40 ms cadence, subject to event-loop availability. The stable state is repainted
once. Actual show/paint events or another `Pulse` resume eligible animation;
there is no idle timer polling for a future preference or visibility change.
The indeterminate segment uses the track height even when marker labels make
the control taller. Marker positions and interaction geometry stay unchanged.

## Verification status and required hosted checks

This candidate has source inspection only. No local compilation, tests, product
execution or captures were performed. Hosted compilation and actual timer,
visibility, cancellation and rendered behavior remain unverified.

The focused hosted native fixture should exercise the production `ProgressBar`
with an event loop: Pulse while visible; reduction before Pulse and during a
sweep; explicit and ancestor hide/show; Disable; SetValue/Reset; destruction with
a queued tick; and motion re-enabled followed by an actual paint. Observe timer
state and numeric value separately. Check that no repaint stream continues while
hidden, disabled or reduced, and that the stationary indicator is visible.

For real notifications, verify a regular message survives its readable interval,
hover and overflow defer dismissal, reduced motion skips only the fade, normal
motion still fades, persistent warnings stay visible, and cancellation invokes
the real callback immediately. Hide or minimize the canvas during fading and
verify that its scheduler does not keep requesting animation frames. These
checks must use actual production objects, not duplicate policy assertions.

Retain a temporal sequence and per-input captures bound to source, executable
hash, language, theme, actual DPI and viewport. Cover English, Cantonese and
bilingual modes, light/dark, normal/minimum dimensions and 100/125/150/200%.
Existing installed cancellation evidence is still required independently; no
physical print or transfer is needed for this motion slice. Notification
entrances, relocation transitions and other widget families are outside this
bounded change and remain in the shared interface-motion inventory.
