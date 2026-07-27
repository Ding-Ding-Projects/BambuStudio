# Project version history

Local, Git-backed version history for every project: complete `.3mf` snapshots
committed into isolated bare repositories, browsable and restorable from the
app. Nothing is synced or pushed anywhere; no `.git` ever appears in the
user's own folders.

## Behavior

- **Capture**: edits and settings changes schedule a debounced snapshot
  (`Plater::priv::schedule_project_history_capture`); manual saves capture the
  just-saved archive. Automatic captures export deterministically
  (`SaveStrategy::Deterministic`) so unchanged projects dedupe to the same
  blob.
- **Storage** (`src/libslic3r/ProjectHistoryManager.{hpp,cpp}`): libgit2
  v1.9.3 bare repositories under
  `<data_dir>/project_history/v1/<sha256(project identity)>`, one full `.3mf`
  blob per commit, written by a single serialized worker thread. Untitled
  sessions get a per-session identity that migrates on Save As.
- **Browse/restore**: **File ▸ Version history…** and the topbar `main •`
  history chip open the MD3 `ProjectHistoryDialog` (commit list with message,
  time, size). A `SearchField` above the list filters versions by commit id,
  message, or timestamp — plain text by default, with the shared `.*` regex
  toggle and tune builder; the status row reports "N of M versions match the
  search", and selection/restore always map through the filtered view so the
  restored commit is exactly the highlighted row. Restoring materializes the chosen version to a temporary `.3mf`
  and swaps the live document (`Plater::restore_project_history_snapshot`)
  with a rollback archive kept until the swap succeeds; the original file on
  disk is never overwritten by the restore primitive.

- **Crash-backup preservation**: when the app starts and finds an unsaved
  crash backup, that backup is committed to history **before** the
  "restore your last unsaved project?" prompt appears
  (`Plater::priv::preserve_unsaved_backup_in_history`). Declining the prompt
  deletes the backup directory, so the commit has to happen first — otherwise
  Cancel is the moment the only copy of unsaved work disappears. A toast
  confirms the work is preserved and stays restorable from Version history.
  The snapshot is staged under a real `.3mf` filename (the backup file is
  literally named `.3mf`, which has no extension by path rules, and the engine
  validates extensions on both the identity and the snapshot). Identity is the
  saved project path when the backup has one, otherwise the untitled identity
  encoded in the backup's session-token marker, so recovered work rejoins the
  crashed session's history instead of starting an orphan one.

## Configuration

None required; history is always on for saved projects. Storage lives beside
the app's own data directory (see above), never inside user project folders.

## Failure modes

- **Snapshot/commit failure** → durable failure snackbar with a Retry
  hyperlink (`push_project_history_failure_notification`), deduplicated across
  repeated failures; failed snapshots persist as manifests and are re-adopted
  on restart (`adopt_orphaned_project_history_failures`).
- **Restore-destination inside the managed root** → rejected by the backend
  (defense against self-corruption).
- **Concurrent app instances** → per-instance staging directories avoid
  cross-talk; the worker serializes repository access.

## Security considerations

- Repositories are local-only bare repos; no remotes are configured and no
  network I/O exists in the backend.
- Project identity hashing (SHA-256 of the normalized path) keeps repository
  directory names free of user path content.

## Verification

- Unit tests: `tests/project_history/project_history_tests.cpp`
  (commit/list/restore/migrate).
- Built and linked via `deps/libgit2/libgit2.cmake` +
  `find_package(libgit2 1.9 CONFIG REQUIRED)`; compiled clean in the current
  full build.
- Dialog and menu/topbar entry points captured in the screenshot matrix under
  `docs/screenshots/version-history/`.
