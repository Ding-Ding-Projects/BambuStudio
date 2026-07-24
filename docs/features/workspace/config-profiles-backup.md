# Config profiles & full-data backup

**Surface:** `File ▸ Config profiles & backup…` → `ConfigProfilesDialog`
(`src/slic3r/GUI/ConfigProfilesDialog.{hpp,cpp}`, gate widget
`src/slic3r/GUI/Widgets/SlideToConfirm.{hpp,cpp}`).

## Behavior

- **Export everything…** packs the *entire active data directory* — presets,
  physical-printer hosts, access codes, network-plugin login state, API
  tokens, caches — into a single `.zip` chosen by the user. Nothing is
  redacted: the point is a byte-faithful migration file another PC can
  import. Locked/transient files (live logs, lock files) are skipped rather
  than failing the export.
- **Import backup…** unpacks such an archive into a **new profile** under
  `<data-dir-parent>/BambuStudio-profiles/<name>/`. The live data directory
  is never touched by an import; a zip-slip guard rejects archives with
  path-escaping entries.
- **Profiles** are unlimited sibling data directories. The list (searchable —
  the pill carries the shared regex builder) always includes the active
  data directory as “Current (active)”. **Launch profile** starts a second
  Bambu Studio instance with `--datadir` pointed at the selected profile.
- **Snapshot now / History…** give *every* profile a local, Git-backed
  snapshot history driven by the same engine as project version history
  (`ProjectHistoryManager`): each snapshot is a complete zipped image of the
  profile committed into an isolated bare repository under the profiles
  root. Restoring **always creates a new profile** (`<name>-restored…`);
  nothing is overwritten or rewound.

## Security considerations

- The transfer card is styled with the MD3 error-container roles and states
  explicitly that the archive **contains secrets** (printer access codes,
  login state, tokens, physical-printer passwords).
- Both Export and Import sit behind a **slide-to-confirm** gate
  (`SlideToConfirm`): a deliberate drag (or arrow-key walk — the control is
  fully keyboard operable: Right/Up advance, End completes, Home resets)
  must complete before the buttons enable; each completed action re-arms
  the gate. A stray click can never export or import.
- The archive itself is **not encrypted** — the dialog says so and tells
  the user to store/transfer it safely. Snapshots and history repos stay
  local; nothing is synced or pushed.

## Failure modes

- Unwritable destination / full disk → explicit error, gate re-arms.
- Unreadable archive, unsafe (path-traversal) archive, or unwritable
  profile folder → import aborts with a specific message; no partial
  profile is activated.
- History engine unavailable (repo init failure) → snapshot/history
  buttons disable; export/import keep working.

## Verification

- Built headlessly (Mesa llvmpipe) and driven via the Lowlevel MCP cheap
  CLI: dialog opens from the File menu, slide gate arms via keyboard `End`,
  export writes a zip whose entry count matches the data dir’s regular
  files, import round-trips the same zip into a new profile listed
  immediately.
- Language modes: all user-facing strings are in the curated `yue_HK`
  catalog (`compile_translation.py --check` green, 494 entries).
