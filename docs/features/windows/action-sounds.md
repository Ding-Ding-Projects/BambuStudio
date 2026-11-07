# Action sounds

Open **Settings > Other > Sound effects** to enable or disable short outcome
sounds. The switch is searchable, appears in the command palette, saves through
the existing preference store, and takes effect immediately. Its initial value is
on, preserving the previous vocabulary-drop sound behavior. Existing stored
choices are not replaced during upgrades. Scheduled settings can override it.

The selected actions are deliberately limited:

| Completed action | Cue |
| --- | --- |
| Valid vocabulary drop or Settings upload, after cache replacement and refresh | Windows `Media/tada.wav` |
| Rejected vocabulary drop/upload or unsuccessful Clear | Windows `Media/Windows Critical Stop.wav` |
| Successful Settings Clear, after original wording is restored | Windows `Media/Windows Notify System Generic.wav` |

These are installed operating-system assets, not downloaded or redistributed
audio. A missing or unsupported file stays silent. Canceled file selection and
School-mode suppression do not request a cue. Existing model-drop classification
is unchanged: normal `.3mf` and `.stl` drops are not vocabulary rejections.
Unsupported non-JSON drop feedback remains outside this change.

All selected cues share one non-blocking worker and a separate WinMM output
handle. They play serially, so cues do not overlap. Up to 32 pending requests are
retained in order; a request exceeding that bound is unavailable and stays silent,
without changing its action result. This bounded overload case does not promise
audible feedback for every request. Changing the sound or quiet controls cancels
the active cue and pending requests. Scheduled effective-preference changes do the
same. Cancellation affects only this service's output handle, not a melody preview
or another application's audio.

Quiet narration and the configured narrator quiet hours also silence action
sounds. When Respect Focus Assist is enabled, notification quiet/busy,
presentation, full-screen, absent-user and application-controlled notification
states suppress cues. Quiet-hour and operating-system-state changes are checked
during playback. There is no separate general in-app volume preference; existing
system/session volume and mute continue to apply. The service never changes host
audio settings. Turning narration itself off does not disable action sounds.

The playback worker receives only an enum and non-sensitive settings. It never
receives an imported filename, mapping or payload. Audio failures never alter
validation, persistence, visible results, celebrations or assistive announcements.
Other platforms currently retain silent behavior for these cues.

## Verification and limits

- `node tests/action_sounds_behavior.test.mjs` compiles the actual service with a
  silent platform adapter. It runs real admission, WAV parsing, worker ordering,
  cancellation and the production scheduler's effective-update prefix. Cases cover
  quiet hours across midnight, manual quiet, Focus Assist, effective overrides,
  stored-setting reconstruction, disabled playback, repeated outcomes, unavailable
  devices, malformed WAV data, bounded queue and disabling during device opening.
- `--mutant=scheduled-cancel` and `--mutant=submit-admission` each produce a failing
  assertion when their respective protection is removed from the compiled copy.
- `node tests/action_sounds_outcomes.test.mjs` executes the actual Settings upload,
  Clear and drop callbacks with neutral loader/UI doubles. It checks exactly one
  cue request after each completed result, repeated drops, cancellation, School
  suppression, and preservation of model-drop classification.

These checks emit no host audio. They do not prove native device playback,
production configuration writes across application restart, or rendered Settings
interaction. Native build, localized and keyboard operation, real restart,
mute/volume behavior, installed audio assets and device-unavailable acceptance
remain unverified until exercised in an authorized environment.

The service also passed an object-only compilation with the installed MSVC and
Windows SDK, replacing only GUI configuration access with a compile stub. This
checks real platform declarations, not full application linkage or playback.
