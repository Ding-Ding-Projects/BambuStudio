# Bulk actions

Every list, table, grid and collection in the application supports selecting
many items at once and acting on the whole selection. Selecting one item and
repeating an action forty times is the app failing to do its job.

## Behaviour

### Selection model

All surfaces share one selection model, `Bulk::BulkSelection<Id>`
(`src/slic3r/GUI/Bulk/BulkSelection.hpp`). The selection is expressed over
stable item ids, never row indices, so it survives a repopulate, a re-sort and
a filter change:

| Gesture | Effect |
|---|---|
| Click | Toggle one item |
| Shift+click | Select the inclusive range between the last clicked item and this one, in display order |
| <kbd>Ctrl</kbd>+<kbd>A</kbd> | **Select this page** — every row currently rendered |
| <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>A</kbd> | **Select all N matches** — every item the current filter yields, rendered or not |
| <kbd>Ctrl</kbd>+<kbd>I</kbd> | **Invert selection** within the current matches; items hidden by the filter are never toggled behind your back |
| <kbd>Delete</kbd> | Open the bulk delete review for the selection (never deletes directly) |

Select-all buttons always state which scope they mean ("Select this page (12)"
versus "Select all 340 matches"). A filter change keeps the selection: items
that are selected but hidden stay selected and the counts line says how many
are hidden. Items that no longer exist are dropped from the selection
(`retain()`), so a stale id can never be acted on.

Search bars on these surfaces are the shared `SearchField`, so plain text is
the default and the `.*` toggle or the anchored regex builder switches the
filter to a bounded regex. Selection composes with the filter: "select
everything matching this query" is <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>A</kbd>.

### Reviewable preview before anything happens

Every bulk action opens `Bulk::BulkActionPreviewDialog` first
(`src/slic3r/GUI/Bulk/BulkActionPreviewDialog.{hpp,cpp}`). The dialog is a
Material Design 3 shell that states:

- the action name in the header;
- **N selected / M will change / K skipped** — derived from the plan, never
  typed by hand, so the three numbers always add up;
- one plain sentence of consequence (what happens to a changed row);
- a reviewable list (item, what it becomes for transforms, outcome, details)
  with its own `SearchField` and regex builder to filter a long preview;
- Cancel and Proceed. Proceed is disabled, with a tooltip saying why, while
  nothing would change.

Each row that will not be touched names its reason ("Skipped: file already
exists", "Skipped: preset is in use"). The surface applies only the rows the
plan marks as changing, and reports skipped and failed items afterwards
instead of claiming the whole batch succeeded.

### Destructive bulk actions

A plan marked destructive routes Proceed through the two-key
`SuperConfirmGate` anchored on the Proceed button: both keys, then the
full-range slide, with the changed items listed as the affected set and the
exact count stated. The gate's Emergency exit and <kbd>Esc</kbd> return focus
to the Proceed button. Nothing is deleted from the preview dialog itself; the
gate's authorization is the only path to the surface's delete routine.

### Rename by pattern

`Bulk::BulkRenameDialog` (`src/slic3r/GUI/Bulk/BulkRenameDialog.{hpp,cpp}`)
renames a selection through the engine in `BulkRenamePattern.hpp`:

| Field | Meaning |
|---|---|
| Pattern | `{name}` keeps the current name, `{n}` numbers from the start index, `{i}` from 0, `{stem}` and `{ext}` split a file name, `{{` and `}}` are literal braces |
| Find / Replace | Every occurrence is replaced. The find field is a `SearchField`: its `.*` toggle or regex builder enables ECMAScript regex with `$1`..`$9` back-references and case sensitivity |
| Start `{n}` at / Zero-pad | Running-index start and width |

The before → after preview re-plans on every keystroke. A row is skipped when
the name would not change, the result would be empty, or the regex is
malformed; a **collision** (two rows producing the same name, or a result
equal to a name the collection already holds) disables Rename and is named on
both rows so neither half of a duplicate lands. Pattern, find and replace are
capped at 512 characters and names at 4096, and a regex error is reported as
an invalid row rather than thrown.

### Long-running actions

`BulkActionPreviewDialog::RunWithProgress` runs the apply loop behind the kit
`ProgressDialog` with elapsed and remaining time, naming the current item
("3 of 40: cube.stl"), and a Cancel that stops after the current item. The
caller receives how many steps completed, whether the run was cancelled and
how many steps failed, and reports all three in a non-blocking notification.

### Undo

Where a surface already records history, a bulk action goes through the same
path: object-list bulk rename and bulk delete take a single undo/redo
snapshot; a notification-centre delete is preceded by the export that keeps a
copy; preset and profile snapshots are recorded by the local Git-backed
history those surfaces already own. Surfaces where undo is impossible say so
in the consequence line.

## Per-surface wiring

Every surface below uses the shared `BulkSelection`, the reviewable preview
and, for a destructive plan, the two-key gate. "Not offered" entries are
deliberate and say why, so a missing action reads as a decision rather than
an oversight.

| Surface | File | Selection | Bulk actions wired | Not offered (reason) |
|---|---|---|---|---|
| Object list | `GUI_ObjectList.cpp`, `GUI_Factories.cpp` (context-menu **Bulk** group) | Native multi-select; **Select all objects** (Ctrl+A), **Select all matches** (Ctrl+Shift+A; the list has no filter, so it equals select all and says so), **Invert selection** (Ctrl+I) | **Bulk rename…** (`BulkRenameDialog`, one undo snapshot), **Bulk delete…** (preview, destructive gate, one undo snapshot), **Bulk export…** (one STL per object into a chosen folder via `Plater::export_object_stl`, existing files skipped, progress with cancel) | Move/copy/tag: objects have no such operations on this surface |
| Sidebar filament (ink) rows | `Plater.cpp`, `BulkFilamentDialog.cpp` | Per-slot checkboxes, select-all, **Invert selection** | The staged batch (apply preset, apply colour, delete slots, add filaments) is shown as one plan with per-slot outcomes; a batch that deletes slots is destructive and passes the gate; the last remaining slot is a named skip | Export: filament slots are project state, exported with the project |
| User presets | `UserPresetsDialog.cpp` (also reachable from the preset combo's edit menu, **Batch Preset Management…**, `PresetComboBoxes.cpp`) | Checkbox rows; **Select visible** (Ctrl+A), **Select all matches** (Ctrl+Shift+A), **Invert** (Ctrl+I), Delete key | **Delete** (preview, destructive gate), **Export selected…** (one `.json` per preset into a chosen folder, existing files skipped, progress with cancel), **Rename selected…** (`BulkRenameDialog`, collisions against every preset name refused, progress with cancel) | Move/copy: presets belong to one collection each |
| Project version history | `ProjectHistoryDialog.cpp`, `ProjectHistoryManager.cpp` | Native multi-select; **Select visible (N)** (Ctrl+A), **Select all N loaded/versions** (Ctrl+Shift+A, wording states whether older versions are still unloaded), **Invert** (Ctrl+I) | **Export selected…** (each version as its own `.3mf` into a chosen folder, progress with cancel), **Label selected…** (`label_version`, a lightweight tag per version; a label a version already carries is a named skip) | Delete: history is append-only by design. Restore: single version only, the button's tooltip explains why |
| Notification centre | `NotificationCenterPanel.cpp`, `NotificationHistory.cpp` | Checkbox rows with shift-range; **Select this page (N)** (Ctrl+A), **Select all N matches** (Ctrl+Shift+A), **Invert selection** (Ctrl+I), **Clear selection**, Delete key | **Dismiss selected**, **Export…** (honours the active filter, four formats), **Delete selected…** (preview, destructive gate; the export that keeps a copy is offered first) | Rename/move: entries are immutable log records |
| Multi-machine manager | `MultiMachineManagerPage.cpp` | Card checkbox with shift-range across the rendered page; **Select this page**, **Select all matches** (every device matching the search across all pages), **Invert selection**, **Clear** (Ctrl+A / Ctrl+Shift+A / Ctrl+I) | **Export selected…** (name, id, model, status, task, progress as JSON or CSV, preview first) | Delete/rename: devices are bound to the account and named on the printer; the page does not own them. Bulk send-to-print already lives on the multi-machine task page |
| Config profiles | `ConfigProfilesDialog.cpp` | Native multi-select; **Select visible** (Ctrl+A), **Select all** (Ctrl+Shift+A), **Invert selection** (Ctrl+I) | **Snapshot selected** (one complete snapshot per profile through the preview and a cancellable progress dialog), **Export list…** (name, data folder, last snapshot as JSON or CSV) | Delete: there is no delete on this surface, single or bulk; a profile is removed by deleting its folder outside the app. Launch stays single-row because it starts another instance |

Shortcuts shown in each surface's tooltips are the ones bound in that
surface's `wxEVT_CHAR_HOOK` handler; a focused text control keeps Ctrl+A for
text selection and Delete for editing, so the list shortcuts only fire when the
list itself has focus.

## Configuration

There is nothing to configure. Shortcuts are fixed to the set above and are
shown in each surface's tooltips and context menu so the displayed shortcut is
the one that actually fires in that surface.

## Failure modes

- Empty selection: bulk buttons are disabled with a tooltip stating that a
  selection is needed.
- Nothing would change: Proceed is disabled and every row shows its skip
  reason.
- A step fails during a long run: the run continues, the failure is counted
  and named in the closing notification; a cancel stops after the current item
  and reports how far it got.
- Rename collisions or an invalid regex: Rename stays disabled and the error
  is stated under the counts line; typed input is never discarded.
- A destructive gate cancelled at any stage performs nothing and returns focus
  to the originating button.

## Security considerations

Regex evaluation for filtering uses the bounded `SearchField::MatchPass`
(pattern and subject size caps, timeout, worker isolation). The rename engine
uses `std::regex` with hard size caps on pattern and subject; it never runs
patterns against anything but the selected names, and nothing typed into a
bulk dialog is transmitted or persisted. Exports write only to a path the user
chose in a native dialog and never overwrite an existing file silently — an
existing file is a skipped row in the preview.

## Verification

- `tests/bulk_actions/bulk_actions_tests.cpp` (Catch2, pure C++17): select-all
  page vs all-matches, invert scoped to the universe, shift-range in both
  directions, retain across filter change, rename placeholders / padding /
  stem+ext, plain and regex find/replace, spec validation, collisions on both
  sides, empty and unchanged skips, plan count arithmetic. Build with the
  `bulk_actions_tests` CMake target or the hand build described in the
  handoff; last run: all 14 cases, 75 assertions passed.
- `tests/notification_center/notification_center_tests.cpp` still exercises
  `NotificationHistory::Selection`, which is now an alias of the shared model;
  last run: 11 cases, 154 assertions passed.
- Every changed `.cpp` passes `cl /Zs` with the GUI project's include set.
- UI behaviour (preview dialog, gate routing, progress cancel) is verified by
  driving the built application; captures belong to the release that ships it.

## Suggested articles

- [Notification centre](notification-center.md) — the first surface to carry
  the shared selection model and bulk delete through the gate.
- [Project version history](project-version-history.md) — bulk export of
  versions; why bulk delete is not offered there.
- [Config profiles & full-data backup](config-profiles-backup.md) — bulk
  snapshots and list export.
- Destructive-action super confirmation — the two-key gate every destructive
  bulk action passes through.
