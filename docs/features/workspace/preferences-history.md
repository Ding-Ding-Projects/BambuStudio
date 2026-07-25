# Preferences auto-history

**Surfaces:** automatic (every settings save); browser in
`File ▸ Config profiles & backup… ▸ Preferences history…`
(`src/slic3r/GUI/PreferencesHistory.{hpp,cpp}`, UI in
`ConfigProfilesDialog`).

## Behavior

- Every successful `AppConfig::save()` fires a save observer
  (`AppConfig::set_save_observer`, installed at `GUI_App::post_init`) that
  schedules a **debounced (2s) snapshot** of `BambuStudio.conf` into an
  isolated local Git repository — the same `ProjectHistoryManager` engine and
  storage root as config profiles (`<data-dir-parent>/BambuStudio-profiles/`,
  identity `preferences.history`). Identical snapshots dedupe inside the
  engine, so a burst of saves costs at most one commit.
- **Preferences history…** lists the snapshots (timestamp, message, commit
  id). Restoring writes `BambuStudio.conf.restored-<commit>` **beside** the
  live file — the live configuration is never replaced under a running app;
  the status line tells the user to swap the file while the app is closed.

## Failure modes

- Repository init failure → the observer degrades to a no-op and the browser
  reports the engine error; the app itself is unaffected.
- History stays local: never synced, never pushed, never inside the exported
  data-directory zip (the storage root lives beside the data dir).

## Verification

- Built into `libslic3r_gui`; headless smoke: changing a preference produces
  a `Preferences change` commit visible in the browser after the debounce;
  restore writes the sibling copy and leaves the live conf untouched.
