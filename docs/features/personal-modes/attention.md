# Attention accommodations

`AttentionModes` implements five independent, default-off settings. Applications
persist `AttentionSettings` and supply concrete callbacks through
`AttentionCallbacks`. A rejected persistence operation leaves the prior effective
state intact. No diagnosis, scoring or medical benefit is implied.

| Accommodation | Callback and expected workspace behavior |
| --- | --- |
| Focus | `focus(bool)` dims surrounding content and emphasizes the current target without removing access |
| Low stimulation | `low_stimulation(bool)` applies reduced motion and calmer presentation; `suppress_nonessential_notifications()` filters only optional notices |
| Time awareness | `elapsed(visible, session, idle)` renders elapsed session and idle time where work occurs |
| One thing at a time | `next_action(visible, text)` shows the user's persisted next action, never an inferred task |
| Momentum | `momentum(visible, idle)` presents a dismissible factual inactivity notice; `dismiss_momentum()` respects the configured snooze |

Call `activity()` for actual user edits or navigation and `tick()` once a second.
The clock is monotonic. Default idle and snooze periods are 20 and 30 minutes;
both are bounded to 1 through 240 minutes. A prompt appears once per idle interval,
unless its explicit snooze expires. User activity dismisses it and restarts the
idle interval. The next-action text is bounded to 4096 UTF-8 bytes by the service;
the UI must validate and explain the bound before saving.

Call `platform_reduced_motion()` when the operating-system preference changes.
Turning off low stimulation never cancels that preference. The callback's reduced
motion value therefore includes the platform preference; inspect
`settings().low_stimulation` for color and notification treatment specific to the
application toggle. School and Kids presentation do not override these independent
accommodations. Application integration must explicitly retain them in both modes.

Settings, search and palette entries must use the application's common localized
control system, including each field's own anchored advanced regex builder. The
service itself creates no additional search field or settings surface.
