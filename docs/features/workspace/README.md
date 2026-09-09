# Workspace

Project- and workflow-level features of the native application: how projects are
opened, tracked, versioned, and how the app communicates with the user while
work is in progress.

- [Non-blocking notifications](non-blocking-notifications.md) — informational,
  warning, and error messages surface as corner toasts instead of modal dialogs;
  decision dialogs stay modal.
- [Notification centre](notification-center.md) — the bell on the top bar
  opens a searchable, filterable history of every toast (500 entries, persisted),
  with multi-select, bulk dismiss, bulk export in four formats and a
  slide-to-confirm bulk delete.
- [Bulk actions](bulk-actions.md) — multi-select (click, shift-range,
  Ctrl+A page / Ctrl+Shift+A all matches / Ctrl+I invert) on every list, a
  reviewable "N selected / M will change / K skipped" preview before any bulk
  action, rename-by-pattern with live preview and collision detection, and
  destructive batches routed through the two-key super confirmation gate.
- [Project version history](project-version-history.md) — local, libgit2-backed
  snapshots of every project, browsable/restorable from File ▸ Version history
  and the topbar history chip.
- [Browser-like project tabs](project-tabs.md) — one tab per project with
  snapshot-based switching, plus the new-tab and close affordances.
- [External editor](external-editor.md) — configurable "Open in External
  Editor" for the current project folder, with editor auto-detection.
- [Config profiles & full-data backup](config-profiles-backup.md) — export the
  entire data directory (secrets included, behind a slide-to-confirm gate),
  import it on another PC as a new profile, keep unlimited profiles, and give
  each one local Git-backed snapshot history.
- [Preferences auto-history](preferences-history.md) — every settings change
  commits BambuStudio.conf into an isolated local Git repo (debounced,
  deduped), with a browser and restore-beside-the-live-file semantics.

## Postman collections

Not applicable. These are desktop workspace features with no HTTP or API
surface, so no Postman collection is provided for this category.
