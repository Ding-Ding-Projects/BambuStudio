# Browser-like project tabs

A browser-style tab bar above the workspace: one tab per project, a `+`
new-tab button, per-tab close, and snapshot-based switching — within the
slicer's single-document architecture.

## Behavior

- The tab strip (`src/slic3r/GUI/ProjectTabBar.{hpp,cpp}`) shows one tab per
  open project (grouping: a tab *is* a project). The active tab is highlighted
  with the MD3 Primary underline treatment.
- `ProjectTabBar` is the project face of the shared browser-style strip
  (`Widgets/TabStrip`, see [Tabbed settings and the shared tab strip](tabbed-settings.md)),
  so the project tabs carry the whole tab contract: a **dock edge** (Top by
  default; Left / Right turn the bar into a side rail beside the workspace,
  Bottom puts it under it — `MainFrame::place_project_tabbar` re-places it),
  an **overflow menu** when tabs exceed the width, **drag and keyboard
  reorder**, **pinning** (pinned projects keep no close affordance and are
  excluded from bulk closes by default), **grouping** with the
  "Move into group…" picker, the **four tab searches** (this strip, one group,
  groups by name, and the master search across every strip including the
  Preferences sections), the two **bulk-close** actions, context menus that
  show their shortcuts and carry **Edit tab appearance…** / **Edit group
  appearance…**, and `tablist` / `tab` accessibility roles whose arrow keys
  follow the strip's orientation (Left / Right when docked top or bottom,
  Up / Down when docked left or right).
- Activation is host-confirmed: a click, Enter, an overflow pick or a search
  hit only *asks* (`EVT_PROJECT_TAB_SWITCH`); the strip marks the tab active
  once `MainFrame::switch_project_tab` has saved the outgoing snapshot and
  loaded the incoming one, so a failed switch never leaves the strip pointing
  at a project that is not on screen.
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

The strip layout — tab order with each tab's file path, pinned state, groups,
collapsed state, dock edge and active tab — is one JSON value under
`[tab_strips] projects` in `BambuStudio.conf`. The legacy `[project_tabs]` and
`[tab_groups]` sections are imported once when no layout exists. Each tab's
project retains its own version history through the per-project identity (see
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
