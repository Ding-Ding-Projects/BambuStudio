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
