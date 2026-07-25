# Smart home: TTS narrator, Home Assistant, alert lights

**Surfaces:** `File ▸ Smart home…` (`SmartHomeDialog`), runtime modules
`TtsNarrator` and `HomeAssistant` (`src/slic3r/GUI/`).

## TTS narrator

- **OFF by default** (`narrator_enabled`, toggle in the Smart home dialog),
  per the narrator rules: serialized queue (one utterance at a time, a
  superseded queued line of the same category is replaced, never stacked),
  20 s per-category cooldown, **error lines always speak** and are always
  plain and accurate.
- Narrates printer state changes (started / finished / paused / failed) and
  printer error codes, polled from the selected machine.
- Output: Windows SAPI voice locally, plus every configured Home Assistant
  **announcement speaker** (`ha_speakers`) via `tts.speak`.

## Home Assistant

- Connection in the dialog: base URL + long-lived token (stored in
  `BambuStudio.conf` — the config-export secrets warning covers it). All
  calls are async and log-only on failure: an absent HA never nags.
- **Entity browser:** shared `SearchField` (regex builder included) over a
  listbox of `media_player.*` and `light.*` entities with friendly names
  and live state.
- **Media controls:** previous / play-pause / next and a volume slider for
  the selected media player (`media_player` services).
- **Announcement speakers:** "Use as announcement speaker" adds the
  selected player to `ha_speakers` — the narrator and the filament scanner
  speak there too.

## Alert lights (Philips Hue and friends)

- "Use as alert light" adds any `light.*` entity to `ha_lights`. Toggles:
  flash **red on printer errors**, pulse **green on print finish**.
- **Stuck-colour guard:** these are real room lights, so before flashing
  the app snapshots them into a Home Assistant scene
  (`scene.bambustudio_light_restore`) and restores that snapshot ~4 s later
  (fired twice; idempotent). Even if the app dies mid-flash, the snapshot
  scene survives inside HA for manual restore.

## Verification

- Compiles into `libslic3r_gui`; dialog opens, browser/search/toggles work
  against config. Live HA calls need a reachable instance — recorded as a
  pending environment pass (no HA on the build host).
