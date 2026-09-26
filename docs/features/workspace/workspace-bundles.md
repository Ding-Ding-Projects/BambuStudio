# Workspace bundles and planning

A `.bambu-workspace` file is a versioned ZIP container for a related set of
projects. Its `Metadata/workspace.json` manifest has a stable bundle ID,
member IDs, overview title and notes, checklist, and planned print slots.
Member projects live under `Members/<id>/project.3mf`; editable source files
live under `Sources/<id>/<relative-path>`. Each project 3MF is copied once,
including any history already embedded in that 3MF. The workspace does not
duplicate a project's history as a separate pack.

The Project screen has a native Workspace subview beside its existing online
project view. The native subview provides overview, files, checklist, notes,
and month/agenda tabs. It can open and save a bundle, add an owned project 3MF
and editable source, edit and reorder checklist items, add planned slots, and
export checklist or calendar data. Switching views does not replace the
current model on the print canvas. Opening a member into that canvas and
writing its dirty state back into the bundle require the application-level
handoff described below.

## Behavior

- A bundle save hashes each owned file, stages the ZIP beside the destination,
  reopens it, verifies every listed entry, and atomically replaces the
  destination. A failed save leaves the previous file and pending ZIP available
  for recovery.
- Loading validates the entire ZIP before extracting any entry. Extraction
  goes only into a newly created child of the caller's private staging root.
  Callers own cleanup of a successfully loaded staging directory.
- Checklist items have stable IDs, order, completion, optional date-only
  deadlines, and links to a member or planned slot. Deadlines carry their
  chosen UTC offset for 09:00 local reminders, including daylight-saving
  transitions. Planned slots store UTC start and end instants, an IANA zone
  label, and the offset selected for their wall time.
- The planner warns when a printer ID is unavailable or two enabled slots for
  one printer overlap. It never submits a print. While the application is
  running, the workspace panel checks reminders once per minute and when it
  opens or reappears. It uses the existing nonblocking notification surface.
  A resumed or reopened workspace groups missed reminders into one catch-up
  notice. Its last delivered check is stored locally by bundle ID. Snooze and
  dismissal update the bundle immediately when it has a save path, while an
  unsaved new workspace still needs an explicit save. The defaults are 15
  minutes before a planned slot and 09:00 local time for a date-only deadline.
- Date-only deadlines remain dates in the bundle and iCalendar export. A
  reminder converts 09:00 on that date using the stored UTC offset. Timed
  slots keep UTC instants and a named time zone. The editor asks separately
  for start and end offsets, and explicitly asks the user to confirm offsets
  for non-UTC zones, especially when a slot crosses a daylight-saving change.
  The current editor does not resolve IANA time-zone rules automatically, so
  it cannot detect an incorrect manually entered offset.
- Checklist JSON and CSV exports and UTC iCalendar exports contain planning
  data only. They do not include project files, credentials, or print commands.

## Limits and validation

The manifest is capped at 1 MiB, the ZIP at 256 entries and 2 GiB expanded,
and each file at 1 GiB. The loader rejects unlisted or duplicate entries,
unsafe paths, invalid IDs and types, unsupported versions, changed lengths or
SHA-256 hashes, and excessive compression ratios. ZIP CRC verification is
performed while each entry is streamed for hashing.

## Verification

The initial backend revision passed four focused cases and 58 assertions for
round-trip IDs and files, traversal rejection, previous-file preservation
after a failed save, missing-printer and overlap warnings, reminder catch-up,
and JSON, CSV, and ICS exports. Later source validation and native UI edits
have not been compiled or run locally because builds moved to CI. The added
tampered-member and invalid-3MF checks are pending that CI verdict. Opening a
member in the active canvas, returning dirty member state to the bundle,
Recent Projects integration remains application handoff work outside this core
module. Live reminder dispatch has source integration but is awaiting a native
build and runtime verification.
