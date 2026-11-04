# Completion-aware narration

Narration is off until `narrator_enabled=true`. `say_tracks(english, cantonese,
category)` accepts independent source tracks. `narrator_language` selects `en`,
`yue_HK`, or `both`; the latter speaks English followed by Cantonese, awaiting SAPI
completion before each next track. Printer state and error producers use the
language service's separate narration tracks and format each localized template
independently. Private display replacements are not applied to speech.

Ordinary queued events of the same category replace their older pending counterpart
and respect a 20-second cooldown. Errors bypass cooldown and take priority after the
currently speaking event, preserving the current bilingual pair. The queue bounds
pending work at 64 events, categories at 64 bytes, and each track at 32768 characters.
An exhausted queue reports unsuccessful admission rather than allocating without
limit. Quiet or screen-reader-yield state cancels active and pending speech.

The SAPI adapter enumerates installed voices repeatedly, uses stable token IDs, and
recognizes English language identifiers and Hong Kong Cantonese `0c04`. A missing
selected voice uses a matching-language fallback while preserving the stored choice.
No matching voice is reported as unavailable, never silently replaced with another
language. Selection keys are `narrator_voice_en` and `narrator_voice_yue`; an empty
value means choose automatically. Rate keys `narrator_rate_en` and
`narrator_rate_yue`, and pitch keys `narrator_pitch_en` and `narrator_pitch_yue`, use
the SAPI range -10 through 10 and default to 0. Pitch uses escaped SAPI XML; spoken
text cannot inject tags. SAPI does not provide a portable network-capability
attribute, so the UI must state that capability is unknown, not claim offline use.

The adapter polls completion every 100 ms and refreshes the voice inventory every
three seconds. Settings can call `voices()` for a fresh inventory, `voice_status()`
for each selection, and `delivery_failed()` for delivery status. Missing-language
translations and absent voices remain unavailable states. All changed choice keys
must be saved and recorded in local settings history by the owning controls.

`say_now()` retains explicit preview consent but joins the serialized queue rather
than interrupting speech. `set_quiet(quiet, screen_reader_active)` is the application
hook for quiet hours, reduced sound and assistive-technology coexistence. `shutdown()`
must run on the GUI thread before application destruction to stop the timer and
release COM resources.

The existing Home Assistant announcement mirror remains available to users who
configured `ha_speakers` and a connection. Each selected source-language track is
forwarded through `HomeAssistant::speak_on_speakers`, even when its local SAPI voice
is unavailable. Private display replacements are never applied before forwarding.
Quiet and screen-reader-yield settings suppress dispatch to both routes.

This is a separately configured external mirror with **unverified remote playback
completion**, not part of the local non-overlap guarantee. The current Home Assistant
adapter returns `void` after submitting a `tts.speak` service request, has no
playback-completion callback, and does not observe per-player completion. Service
dispatch cannot establish when remote speech ends; remote players may overlap or
finish at different times. `external_mirror_status()` reports `Unconfigured` or
`PlaybackCompletionUnavailable` for the owning settings/status control to label
this distinction. It does not claim network delivery or playback success. Adding
reliable per-player completion remains separate integration work.

## Event narration, tracks, tone and quiet hours

All event narration enters through `TtsNarrator::narrate(Narration::Event)` on the GUI
thread. The pure logic lives in standard-library-only headers under
`src/slic3r/GUI/Narration/`:

- `NarrationEvents.hpp` lists the events and their three tones. An event carries at
  most one integer (a printer error code). It never carries a file path, printer name,
  account name or token, so an utterance cannot disclose private data.
- `NarrationTransitions.hpp` turns printer snapshots into events on state transitions
  only (print started, paused, resumed, finished, failed, cancelled, filament runout,
  AMS error, other printer error, door opened, door closed) and de-duplicates identical events inside a 30 second
  window. The first sight of a printer only records its state, so opening the app next to
  an old finished job or a standing error stays silent. Stage 6 (filament runout), error
  `0x0300400C` (task cancelled) and error module `0x07` (AMS) follow community-documented
  values and are not yet confirmed on hardware. Door opened and door closed are spoken
  only on printers that report door-open detection support, and only when the door
  changes while the app is watching: a door already open at startup, or a reading that
  disappears and returns, stays silent. The door reading comes from a community-documented
  status bit (bit 23 of `home_flag` or `print.stat`), so the door events are beta, like
  the door reading on the print monitor.
- `NarrationPolicy.hpp` holds the settings keys and the quiet decision.
- `VoiceCatalog.hpp` classifies voice language attributes and merges voice sources.

### Application events

Slicing finished (once per slice job, on the last plate of a slice-all run), slicing
failed, export finished, an update ready to install, and any error notification are
spoken as short summaries. The notification text itself is never spoken.

### Tone follows the funny level

Each event has three English source strings and three Cantonese translations. Funny
levels 1 and 2 choose the plain wording, level 3 the friendly wording and levels 4 and 5
the playful wording, separately for English and Cantonese. Every variant carries the same
facts; spoken errors still name the failure and what to do about it.

### Independent tracks

Events travel on three tracks: printer, application and accessibility (voice previews and
the filament scanner). Each track has its own switch (`narrator_track_printer`,
`narrator_track_app`, `narrator_track_accessibility`, all on by default once the master
switch `narrator_enabled` is on). The speech queue keys replacement and cooldown by track
and category, so an event on one track never replaces or throttles an event on another.
Errors keep priority and are never replaced. The legacy `say()` and `say_now()` functions
are deprecated wrappers that use the accessibility track; the filament scanner now calls
`say_tracks()` and follows the narration language setting instead of the interface
language.

### Quiet hours and Focus Assist

`NarratorEnvironment` evaluates, once a second:

| Condition | Result |
| --- | --- |
| Quiet narration switch, active screen reader, or the app's quiet hours (`narrator_quiet_hours_enabled`, `_start`, `_end`, half-hour steps, may wrap midnight) | Held: nothing speaks and queued speech is cancelled |
| Windows Focus Assist quiet time, a full-screen app or presentation mode, while `narrator_respect_focus_assist` is on | Errors only: queued ordinary events are dropped, errors still speak |
| Otherwise | Open |

The Windows state comes from `SHQueryUserNotificationState`. It does not distinguish
Focus Assist priority-only from alarms-only, so both are treated alike. A voice preview
the user starts works with the master switch off, but never while the state is Held.

### Voice picker, preview and OneCore voices

Preferences lists the installed SAPI voices per language and has a **Preview** button for
each language that speaks a short sample with the effective voice. The status line states
the effective voice, a kept-but-missing selection, and whether the effective voice is an
online (network-backed) voice. Windows publishes natural and OneCore voices, including
Hong Kong Cantonese, under a separate registry inventory. `enumerate_onecore_voices()`
lists them read-only. **This build cannot speak with a OneCore voice**, because playback
goes through the classic SAPI interface; when a Hong Kong Cantonese voice exists only
there, Preferences says so instead of hiding it. Speaking with those voices needs a
Windows.Media.SpeechSynthesis playback path, which is not implemented.

### Verification

`node tests/narration/narration.test.mjs` compiles and runs the standard-library checks
(event tones, transitions, de-duplication, quiet hours, Focus Assist, track independence,
voice classification); add `--negative` to break nine rules in scratch copies and require
each break to fail. `node tests/narration/narration_wiring.test.mjs` checks producers,
settings, catalogue completeness and build wiring. Native behaviour in the built
application has not been observed on Windows.
