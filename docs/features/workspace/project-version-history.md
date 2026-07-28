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

- **Crash-backup preservation, verified live end-to-end** (2026-07-27) on the real built binary
  through `.claude/skills/run-bambustudio/`:
  1. Loaded `cube.stl`, waited for the backup `.3mf` to be written (8662 bytes), then hard-killed
     the process — a genuine crash remnant, not a synthetic directory.
  2. Removed the now-stale `lock.txt`, without which the restore prompt did not appear.
     **The reason recorded here at the time was wrong** — see "Why a stale lock suppressed
     recovery" below for what was actually measured.
  3. Pointed `app/last_backup_path` at that directory and relaunched. The
     [restore prompt](../../screenshots/version-history/crash-restore-prompt.png) appeared.
  4. Clicked **Cancel** — the branch that runs `remove_all` on the backup directory.
  - **Result:** the backup directory is gone, and commit `82f6cd1 "Recovered unsaved project"`
    survives in the project-history repo carrying `project.3mf` at **exactly 8662 bytes** — the
    backup that Cancel destroyed. This is the whole point of the feature and it holds.
- The restore path logs a `restore check: last_backup_dir=... has_restore_data=...` line, because
  recovery is silent when it declines and "the prompt never appeared" is otherwise undiagnosable
  after the fact.

### Why a stale lock suppressed recovery (measured 2026-07-28)

The earlier note above blamed `has_restore_data()`'s `catch (...)`. That was a guess, and probing
the exact Win32 sequence `get_process_name()` performs shows it is not what happens:

| Probe | Measured result |
| --- | --- |
| `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, …, dead_pid)` | returns **`NULL`**, `GetLastError() = 87` |
| Is that `INVALID_HANDLE_VALUE`? | **No** — that sentinel is `-1` and is never returned here |
| `GetModuleFileNameExA(NULL, …)` | fails (`err = 6`), leaves the buffer empty |
| `CloseHandle(NULL)` | fails (`err = 6`) — an **invalid-handle close** |
| `GetModuleFileNameExA` on a *live* process, limited access only | **succeeds** — so live-instance detection does work |

A dead pid therefore produced an empty name, the comparison against the running executable did not
match, and the `catch` was never reached. Three real defects were found at that site instead, and
all three are fixed:

1. **Wrong failure sentinel** (`src/libslic3r/utils.cpp`). The `INVALID_HANDLE_VALUE` guard missed
   `OpenProcess`'s actual `NULL`, so a dead pid — the normal input here — reached
   `GetModuleFileNameEx` and then `CloseHandle` with a null handle. Closing an invalid handle
   raises `STATUS_INVALID_HANDLE` under a debugger or with strict handle checking, turning crash
   recovery into a second crash.
2. **Own-pid reuse read as a live owner** (`src/libslic3r/Format/bbs_3mf.cpp`). Windows reuses
   freed pids, so relaunching straight after a crash can hand the new instance the crashed one's
   pid; the check then compared the process against itself and declined recovery. This is the most
   likely explanation for the 2026-07-27 observation. The lock check now rejects a match on the
   current process's own pid, and never lets an empty name compare equal to anything.
3. **Exception escaping into startup** (`src/libslic3r/Format/bbs_3mf.cpp`). `load_string_file()`
   sat outside the `try`, so an unreadable lock file — including the race where it is deleted
   between the `exists()` check and the read — threw out of `has_restore_data()` into the
   `EVT_RESTORE_PROJECT` handler, where an unhandled throw takes the app down at startup. It now
   declines, sets `origin = "<lock>"` so the caller does not delete a backup whose ownership is
   unknown, and logs the reason.

Regression coverage: `tests/libslic3r/test_crash_restore.cpp` (target `crash_restore_tests`, in the
maintained CTest gate). It asserts the `OpenProcess` sentinel invariant directly, covers the
dead-owner, own-pid-reuse, corrupt, empty and unreadable lock bodies, and runs the fixed function in
a **child process with strict handle checking enabled**, so defect 1 fails the suite instead of
staying latent.

> [!WARNING]
> Editing `BambuStudio.conf` by hand to stage this test is a trap. The file ends with a
> `# MD5 checksum` line, and while a stale checksum only logs a warning, **malformed JSON makes the
> app silently fall back to `BambuStudio.conf.bak`** — so the edit appears to be ignored. Write the
> body with a real JSON serializer and recompute the checksum over everything up to and including
> the last `}`.

- Unit tests: `tests/project_history/project_history_tests.cpp`
  (commit/list/restore/migrate).
- Built and linked via `deps/libgit2/libgit2.cmake` +
  `find_package(libgit2 1.9 CONFIG REQUIRED)`; compiled clean in the current
  full build.
- Dialog and menu/topbar entry points captured in the screenshot matrix under
  `docs/screenshots/version-history/`.
