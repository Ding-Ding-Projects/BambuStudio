# Browser-like project tabs

A browser-style tab bar above the workspace: one tab per project, a `+`
new-tab button, per-tab close, and snapshot-based switching — within the
slicer's single-document architecture.

## Behavior

- The tab strip (`src/slic3r/GUI/ProjectTabBar.{hpp,cpp}`) shows one tab per
  open project (grouping: a tab *is* a project). The active tab is highlighted
  with the MD3 Primary underline treatment.
- **Switching** (`MainFrame::switch_project_tab`): the outgoing project is
  serialized to a temp `.3mf` snapshot (`Plater::save_snapshot_to`, full
  Backup-strategy archive), the incoming tab's snapshot is loaded
  (`load_snapshot_from`), and the project identity (file path, dirty state) is
  restored per tab. Tab-sync events are gated during the switch to prevent
  re-entrancy.
- **Closing** (`MainFrame::close_project_tab`): dirty tabs — including
  background (inactive) dirty tabs — prompt for confirmation before closing
  (a genuine decision, so a modal dialog by design).
- **New tab** (`+`): opens a fresh untitled project tab.

## Configuration

None. Tabs are session state; each tab's project retains its own version
history through the per-project identity (see
[project-version-history](project-version-history.md)).

## Failure modes

- **Snapshot save/load failure on switch** → the switch aborts and the current
  project stays live (no partial swap).
- **Restore of a dirty restored tab**: file identity is re-applied after the
  snapshot load (`set_project_filename`), so Save still targets the right
  path (fixed after adversarial review).

## Security considerations

Snapshots are written to the app's temp/staging locations only; closing a tab
deletes its temp snapshot.

## Verification

- Shipped after a three-finding adversarial review (identity restore, outgoing
  snapshot clobber, background-dirty close confirmation) — all fixed.
- Tab strip, new-tab button, active/inactive states captured in the screenshot
  matrix under `docs/screenshots/project-tabs/`.
