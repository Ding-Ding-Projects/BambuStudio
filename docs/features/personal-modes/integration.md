# Application integration checklist

The parent integration owns shared application, preferences, palette, configuration,
catalog and build files. The personal-mode lane changes only its owned service files.
The following work is required before these services can be described as a complete
visible feature.

- [ ] Add `add_subdirectory(personal_modes)` to `tests/CMakeLists.txt` when its platform prerequisites are present.
- [ ] Own one `PersonalModes::SchoolRuntime` on the GUI thread, before rendering translated/private text; include `PersonalModes/SchoolRuntime.hpp`. Its implementation is header-only.
- [ ] In its callback, rebuild language presentation, bilingual registry, settings and palette capabilities, refresh `PersonalVocabulary`, and re-render live windows using the preserved base preferences. Do not overwrite stored language or funny levels.
- [ ] Route every hidden capability through `SchoolMode::available()`, including direct keyboard and programmatic activation. Use only the chosen display name after a rename.
- [ ] Bind enrollment, verification and generation management to `LocalSecurity::Credentials` and `shared_mode_account`. No UI boolean may stand in for credential verification. Provide the shared-record deletion reset disclosure.
- [ ] Show native watcher, fallback polling, unreadable record and corrupt record state honestly. Keep the mode control available.
- [ ] Supply all five `AttentionCallbacks`, persist `AttentionSettings`, register local history and export, drive activity and one-second ticks, and retain those accommodations under School and Kids presentation.
- [ ] Add narrator enabled/language controls and separate English/Cantonese voice, rate and pitch controls. Persist stable IDs and show selected-missing, effective fallback, unavailable and unknown network capability state.
- [ ] Bind quiet/reduced-sound and active screen-reader state through `TtsNarrator::set_quiet`; invoke `TtsNarrator::shutdown` before GUI teardown.
- [ ] Label the configured Home Assistant route as an external mirror with unverified playback completion using `TtsNarrator::external_mirror_status()`. Do not present its dispatch as satisfying the local serialization guarantee.
- [ ] Migrate remaining already-localized `say()` producers to `say_tracks()` so narration language is independent of the visible interface. Printer event producers already use independent tracks.
- [ ] Localize every new control, explanation, status and disclosure, add it to settings/palette search, and give every new search its own adjacent anchored full regex builder.
- [ ] Produce the controls through the approved design route, compile the real application, and capture behavior for language, suppression/restoration, keyboard operation, narrow layouts and high scales. Service tests do not establish those claims.

No new visible surface was authored by this service lane, so it does not claim a
design preview, capture, or complete settings flow. These outstanding integration
requirements are explicit completion blockers, not implicit exemptions.
