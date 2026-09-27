# Notification centre

The notification centre is the reviewable history of every toast the desktop
app has shown: the ImGui corner notifications described in
[Non-blocking notifications](non-blocking-notifications.md) disappear on their
own, and this is where a dismissed one can still be read, searched, exported or
deleted. It opens from the bell on the top bar (`BBLTopbar`, left of the
appearance button) as a non-modal Material Design 3 popover anchored under the
bell.

Source: `src/slic3r/GUI/NotificationHistory.{hpp,cpp}` (model),
`src/slic3r/GUI/NotificationCenterPanel.{hpp,cpp}` (popover),
`src/slic3r/GUI/NotificationManager.{hpp,cpp}` (recording hooks),
`src/slic3r/GUI/BBLTopbar.cpp` (bell + badge).

## Behavior

### Recording

- `NotificationManager` owns one `NotificationHistory`. Every notification that
  becomes a new toast (`push_notification_data`, the branch that appends to
  `m_pop_notifications`) appends one record: id, UTC timestamp (ms), level and
  its stable name (`regular`, `important`, `warning`, `serious_warning`,
  `error`, …), the `NotificationType` enum name, the title (first line of the
  text), the full text (`text1` plus `text2`), a dismissed flag, a seen flag
  and the action taken.
- Progress-bar toasts (`ProgressBarNotificationLevel`) are **not** recorded:
  they re-push on every tick and would drown the list. Updates to an already
  visible toast (`activate_existing`) do not create a second row either.
- A toast is marked dismissed when it leaves the screen for any reason: the
  user closed it, it faded out, or the manager removed it programmatically
  (`update_notifications`, `close_and_delete_self`,
  `remove_notification_of_type`, and the in-place replacements for
  `AssemblyInfo` / `BBLObjectInfo`).
- Clicking a toast's hypertext records the hypertext label as the action taken
  (`on_text_click` / `on_second_text_click`).
- The history is append-only and bounded to 500 entries
  (`NotificationHistory::DEFAULT_MAX_ENTRIES`); the oldest records fall off.
  Ids are monotonic and never reused, even after trimming or a reload.

### Bell and badge

- The bell shows `notifications` when nothing is unread and
  `notifications_active` plus an Error-filled badge with the count (capped at
  `99+`) otherwise. "Unread" means "arrived since the centre was last opened";
  opening the centre marks everything seen and clears the badge.
- Hover paints the same circular ghost disc as the appearance button. The
  tooltip / accessible name reads `Notifications` or `Notifications (N unread)`.
- The bell is guarded on the Material Symbols face exactly like the appearance
  button: without the font neither is added.

### The popover

- An `MD3Dialog` (resizable variant) shown non-modally, positioned under the
  bell and clamped to the display's client area. The header close button and
  <kbd>Escape</kbd> hide it; the bell toggles it. Filter, page and selection
  survive a hide/show.
- **Search**: the shared `SearchField` with the regex builder. Plain text is
  the default; the `.*` toggle or the `tune` popover enable regex, case, whole
  word and multiline. Matching runs through one `SearchField::MatchPass` per
  refresh over title, text, type name, level names and action. The query is
  combined with the level chips and the dismissed toggle, never overriding
  them.
- **Level chips**: All levels · Info (hint, regular, print info) · Important ·
  Warnings (warning, serious warning) · Errors. **Show dismissed** toggles
  whether closed toasts are listed.
- **List**: newest first, one row per record with Level, Time (local), Title,
  Details (remaining lines), Status (Active / Dismissed) and Action taken. It
  is a `wxDataViewListCtrl` in multi-select mode: click, <kbd>Shift</kbd>+click
  range, <kbd>Ctrl</kbd>+click, arrow keys, <kbd>Ctrl</kbd>+<kbd>A</kbd>
  (selects **this page**), <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>A</kbd>
  (selects **all matches**). Rows are rendered 100 at a time; **Show N more**
  extends the page. Every row is focusable and the control carries the
  accessible name `Notification history`; MSW's data view exposes each row's
  cell text to screen readers.
- **Selection row**: `Select this page (N)` and `Select all N matches` are two
  distinct actions with the count in their label, so the user always knows
  whether the rendered slice or every match is being selected. `Invert
  selection` flips within the current matches; `Clear selection` empties it.
  Selection is kept as a set of ids, so it survives auto-refresh, paging and
  filter changes (ids that fall outside the filter stay selected but are not
  counted or acted on).
- **Bulk dismiss**: closes any selected toast still on screen and marks the
  records dismissed.
- **Bulk export**: JSON, CSV, Markdown or plain text, chosen through the save
  dialog's file-type filter. Exports the selection, or every match when nothing
  is selected, always honouring the active filter. Every export states its
  range in a header: `Exported N of M recorded notifications; query: …;
  levels: …; status: …; time span <oldest> to <newest>; exported at …;
  encoding UTF-8; schema v1`. Timestamps are ISO-8601 UTC with milliseconds.
- **Bulk delete**: opens an inline ErrorContainer card that names the exact
  count ("Permanently delete N notification entries …") behind a
  `SlideToConfirm` gate (danger styling, keyboard-operable: arrows walk the
  knob, <kbd>End</kbd> completes, <kbd>Home</kbd> resets) with a Cancel button.
  Deleting also closes any matching live toast. The gate is withdrawn
  automatically if the selection empties while it is open. Focus returns to the
  Delete button afterwards. `// TODO(SuperConfirmGate)` in the panel marks the
  swap to the two-key super confirmation gate once that lane lands.
- **Auto-refresh**: while shown, a 1 s timer compares
  `NotificationHistory::revision()` and repopulates when it changed; the
  selection and page limit are preserved.
- **Empty state**: `No notifications match. New toasts will appear here as
  they are shown.` replaces the list when the filter yields nothing.
- Disabled bulk buttons carry a tooltip naming the unmet condition (`Select
  at least one notification first`, `Nothing matches the current filter`).

## Configuration

- Persistence file: `<data_dir>/notification_history.json` (beside
  `BambuStudio.conf`). Written atomically (`.tmp` then rename) on every change;
  loaded once when the `NotificationManager` is constructed.
- Bound: 500 entries, fixed in code. Page size: 100 rows, fixed in code.
- No user setting disables recording; the centre is part of the notification
  system, not an opt-in feature.

## Failure modes

- Missing history file: normal first run, starts empty.
- Malformed or newer-schema file: not loaded, a warning is logged, the app
  starts with an empty history and the next change overwrites the file.
- Save failure (read-only directory, disk full): logged at warning level; the
  in-memory history keeps working and the next change retries.
- Export write failure: an error toast names the path and asks the user to
  check the folder; nothing is partially claimed.
- Regex that exceeds the bounded-regex budget: `SearchField::MatchPass`
  treats the row as a match (fail-open, as everywhere the builder is used) so
  a bad pattern never hides entries silently.
- Deleting the record of a toast still on screen closes that toast too, so the
  list and the corner never disagree.

## Security considerations

- The history is plain JSON on the local disk; it contains whatever text the
  app put in a toast (file paths, printer names, error strings). It is never
  synced, uploaded or included in bug reports automatically.
- Exports are written only to the path the user picked. CSV fields are quoted
  per RFC 4180 and Markdown pipes are escaped, so a toast text cannot break the
  table or inject a formula-looking cell unquoted.
- Regex evaluation runs through the shared bounded engine (deadline and
  isolated worker), so a pathological pattern cannot hang the UI.
- Toast text is rendered as plain text in the list; nothing is interpreted as
  markup.

## Verification

- `tests/notification_center/` (Catch2 target `notification_center_tests`)
  covers append order, dismiss idempotence, actions, seen/unread, the 500
  bound, erase, JSON persistence round trip and rejection of malformed input,
  filter composition (query × level × status, custom matcher, newest-first),
  select-all-page versus select-all-matches, inverse selection, shift ranges,
  and the CSV / JSON / Markdown / plain-text export shapes including the
  range header. Built and run by hand on 2026-09-08: **154 assertions in 11
  test cases, all passed**.
- `cl /Zs` syntax checks passed for `NotificationHistory.cpp`,
  `NotificationCenterPanel.cpp`, `NotificationManager.cpp` and
  `BBLTopbar.cpp` against the `libslic3r_gui` include set.
- Not yet done in this lane: a full application build and headless captures of
  the bell, the popover, the empty state and the delete gate. Those belong to
  the integration pass.

## Suggested articles

- [Non-blocking notifications](non-blocking-notifications.md) — the toasts
  this centre records.
- [Preferences auto-history](preferences-history.md) — the other append-only
  local record with a browser and filters.
- [Config profiles & full-data backup](config-profiles-backup.md) — the other
  destructive flow guarded by `SlideToConfirm`.
