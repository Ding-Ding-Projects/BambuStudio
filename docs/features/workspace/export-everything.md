# Export everything, in every format

**Surfaces:** every record the app owns routes through one shared
`ExportDialog` (`src/slic3r/GUI/Export/ExportDialog.{hpp,cpp}`). The engine
behind it is wx-free (`src/slic3r/GUI/Export/ExportFormats.hpp`,
`ExportEverything.{hpp,cpp}`) and the per-surface dataset builders live in
`src/slic3r/GUI/Export/ExportDatasets.{hpp,cpp}`.

| Surface | Where the `Export…` control is | Dataset kind |
| --- | --- | --- |
| Project version history | `File ▸ Version history…` → **Export…** button | tabular (one row per snapshot commit) |
| Preferences (`BambuStudio.conf`) | Preferences bottom row → **Export preferences…**; also `File ▸ Export ▸ Export preferences…` | structured (section → key → value) |
| Presets (print / filament / printer) | preset toolbar `download` icon beside Save / Delete on every settings tab | structured (metadata + every option as the `.ini` serializes it) |
| Object list | object right-click menu → **Export object list…**; also `File ▸ Export ▸ Export object list…` | tabular (index, name, printable, instances, parts, modifiers, negative/support volumes, facets, size X/Y/Z, volume names) |
| Print statistics | `File ▸ Export ▸ Export print statistics…` (enabled once the plate is sliced) | structured (per-mode times by move type / role / layer, volumes per extruder, flush, filament changes, filament diameters/densities/costs) |
| Whole data folder | `File ▸ Config profiles & backup…` → **Export everything…** (unchanged, see [config-profiles-backup.md](config-profiles-backup.md)) | ZIP of the directory |

The notification history and the changelog viewer are not yet in this tree;
when their lanes land they get an `Export…` entry through the same dialog
by building a `Dataset` and calling `ExportDialog::run`.

## Behavior

- **Every format for every dataset.** JSON, JSON Lines, YAML, TOML, XML,
  CSV, TSV, Markdown and HTML are all offered every time. The natural
  formats for the datum come first and are marked *recommended*
  (tabular → CSV/TSV, structured → JSON/YAML/TOML, prose → Markdown/HTML).
- **Lossless / lossy badges with the exact reason.** `compute_loss_report`
  inspects the real data, not the format in the abstract: a TOML export is
  lossy only when the data actually contains nulls (TOML has none), an XML
  export only when a string carries a C0 control character XML 1.0 forbids,
  a JSON export only when a number is NaN/Infinity. The format card shows
  *Lost:* and *Notes:* lists before anything is written, and the status line
  repeats the loss text at the moment of export. Nothing is ever dropped
  silently.
- **CSV/TSV never lose nesting.** A structured dataset written as CSV/TSV is
  flattened to `path,type,value` rows with RFC 6901 JSON-pointer paths
  (`/inner/empty list`, `/a~1b`), and empty arrays/objects and nulls keep a
  row, so the tree is reconstructable. Tabular CSV follows RFC 4180 quoting;
  TSV escapes `\t \n \r \\` with backslashes.
- **Every file states its schema, encoding and line ending.** JSON gets an
  envelope (`schema`, `schemaVersion`, `dataset`, `kind`, `encoding: UTF-8`,
  `lineEnding`, `generator`, `columns`, `data`); JSONL a first header record
  with `"_type":"header"`; YAML/TOML/Markdown a comment line; XML attributes
  on the `<export>` root; HTML `<meta>` tags. CSV and TSV have no comment
  syntax, so their header travels in a `<file>.meta.json` sidecar that also
  records the separator, the quoting rule and each column's declared type.
- **UTF-8 always; LF or CRLF by choice; BOM opt-in.** The line-ending choice
  is recorded in the header. The byte-order mark exists only for
  spreadsheets that misread CSV without it and is off by default.
- **Archives are ZIP or 7z, never something else.** ZIP is written in-process
  by the vendored miniz (Deflate) and is never encrypted — the dialog says so
  instead of offering a password. 7z runs the 7-Zip command line found on the
  PC and exposes its whole option surface: method (LZMA2 / LZMA / PPMd /
  BZip2 / Deflate), level (store … ultra), dictionary size, word size, solid
  on/off and solid block size, thread count, split volumes, AES-256 password
  and **encrypted headers**. Every option carries a cost hint (RAM to
  compress and extract, single- vs multi-threaded, what solid costs on
  extraction). With a password set and headers left in the clear the dialog
  shows a red warning that the file names stay readable; it never calls such
  an archive protected.
- **Member paths are relative and safe.** `sanitize_archive_path` strips
  leading slashes and `./`, converts backslashes, and refuses drive letters,
  UNC prefixes, empty segments, control characters and any `..` segment.
  7z members are staged in a fresh temporary directory and added with `*`
  from that directory, so 7-Zip itself never sees an absolute path.
- **Search and regex builder.** The dialog carries the shared `SearchField`
  over format rows and option rows with its `.*` toggle and the full anchored
  regex builder.
- **Native browse control** next to the output path (`wxFileDialog`, filtered
  to the chosen extension); the extension follows the selected format or
  archive. **Locate 7z.exe…** is a second native browse that pins the 7-Zip
  executable in `BambuStudio.conf` (`[export_everything] seven_zip_path`);
  the last export directory is remembered there too (`last_dir`).
- **Non-blocking result.** Success posts the standard "export finished"
  notification with an open-folder action; failure is an inline status line
  in the error colour with the real cause (`7-Zip exited with code 2 (fatal
  error)`, `Refused unsafe archive member path: ../x`).

## Configuration

| Key (`BambuStudio.conf`, section `export_everything`) | Meaning |
| --- | --- |
| `last_dir` | Directory of the last successful export; the default location next time. |
| `seven_zip_path` | Full path to `7z.exe` chosen through *Locate 7z.exe…*; checked before the automatic search. |

7-Zip discovery order: the pinned path, then `7z.exe` / `7za.exe` on `PATH`,
then `%ProgramFiles%\7-Zip`, `%ProgramW6432%\7-Zip`,
`%ProgramFiles(x86)%\7-Zip`, then `%LOCALAPPDATA%\Programs\7-Zip`.

## Failure modes

- **7-Zip not found.** The 7z option stays visible with an honest status
  (`7-Zip not found. Install 7-Zip or locate 7z.exe. Searched: …`), the
  Export button is disabled with that reason as its tooltip, and ZIP and
  plain files keep working. There is no silent fallback from 7z to ZIP.
- **Password mismatch** disables Export with "The two password fields differ."
- **Lossy format chosen.** Allowed — the point is that the user was told.
  The badge, the *Lost:* list and the export-time status line all say what
  goes missing.
- **7-Zip exit code 1** (warning) still counts as written; the warning text is
  appended to the success summary. Codes 2/7/8/255 fail with 7-Zip's meaning
  spelled out.
- **Unsafe member path** (only reachable through a programming error, since
  the dialog names members itself) aborts before anything is written.

## Security considerations

- The 7z password is passed to `7z.exe` as a `-p` argument for that one
  process, never persisted, and redacted to `-p***` in the command line the
  dialog shows afterwards. On Windows a command line is visible to other
  processes of the same user for the lifetime of the 7-Zip process; the
  ZIP path involves no external process at all.
- Preset and preferences exports contain whatever the live records contain.
  `BambuStudio.conf` can hold access codes and tokens exactly as the
  full-data backup does; the export is the user's deliberate action from a
  dialog that names the dataset.
- Archive extraction safety is enforced on the writing side
  (`sanitize_archive_path`) so no archive this feature produces can carry a
  path-escaping member.

## Verification

- `tests/export_everything/export_everything_tests.cpp` (Catch2, target
  `export_everything_tests`, links only `test_common` and `miniz`): escaper
  round trips (JSON, CSV quoting, TSV escapes, XML entities, TOML/YAML
  quoting, Markdown cells, CRLF), JSON envelope/JSONL header, YAML nesting and
  always-quoted scalars, TOML tables / arrays of tables / null omission with
  its loss report, XML structure, CSV/TSV with sidecar, CSV flattening of
  nested data with JSON-pointer escaping, Markdown/HTML rendering and their
  loss reports, NaN/control-character edge cases, archive path sanitizing
  (`..`, drive letters, UNC, empty segments), a miniz ZIP round trip read
  back with the miniz reader, the complete 7-Zip switch mapping for every
  method/level/dictionary/word/solid/thread/volume/password/header
  combination including password redaction, and `run_export` writing a data
  file + sidecar, a ZIP, and the honest "7-Zip not found" error.
- Last hand-run result: `All tests passed (247 assertions in 13 test cases)`.
- Syntax check: every changed `.cpp` compiles under `cl /Zs` with the GUI
  include set (Tab.cpp, GUI_Factories.cpp and MainFrame.cpp need the
  project's precompiled header force-included, as the real build does).

## Suggested articles

- [Config profiles & full-data backup](config-profiles-backup.md) — the
  whole-data-folder ZIP this feature sits beside.
- [Project version history](project-version-history.md) — the first surface
  wired to the dialog.
- [Preferences auto-history](preferences-history.md) — the settings record
  that the preferences export snapshots.
- [Non-blocking notifications](non-blocking-notifications.md) — how the
  export-finished toast behaves.
