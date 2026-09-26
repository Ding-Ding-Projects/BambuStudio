# Workspace bundles and planning

A `.bambu-workspace` file is a versioned ZIP container for a related set of
projects. Its `Metadata/workspace.json` manifest has a stable bundle ID,
member IDs, overview title and notes, checklist, and planned print slots.
Member projects live under `Members/<id>/project.3mf`; editable source files
live under `Sources/<id>/<relative-path>`. Each project 3MF is copied once,
including any history already embedded in that 3MF. The workspace does not
duplicate a project's history as a separate pack.

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
  one printer overlap. It never submits a print. Reminder polling catches up
  across sleep or restart, and planned-slot reminders can be disabled,
  snoozed, or dismissed. The defaults are 15 minutes before a planned slot
  and 09:00 local time for a date-only deadline.
- Checklist JSON and CSV exports and UTC iCalendar exports contain planning
  data only. They do not include project files, credentials, or print commands.

## Limits and validation

The manifest is capped at 1 MiB, the ZIP at 256 entries and 2 GiB expanded,
and each file at 1 GiB. The loader rejects unlisted or duplicate entries,
unsafe paths, invalid IDs and types, unsupported versions, changed lengths or
SHA-256 hashes, and excessive compression ratios. ZIP CRC verification is
performed while each entry is streamed for hashing.

## Verification

`workspace_tests.exe` currently passes four focused cases and 58 assertions
for round-trip IDs and files, traversal rejection, previous-file preservation
after a failed save, missing-printer and overlap warnings, reminder catch-up,
and JSON, CSV, and ICS exports. Native UI and end-to-end project integration
remain separate verification surfaces.
