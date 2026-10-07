# Workspace

Project- and workflow-level features of the native application: how projects are
opened, tracked, versioned, and how the app communicates with the user while
work is in progress.

- [Readable dates](readable-dates.md): full month names, localized date text,
  viewer-local timestamps, and truthful compiled-build information.

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
- [Workspace bundles and planning](workspace-bundles.md) — portable grouped
  projects, owned editable sources, checklist, planned slots, reminders, and
  calendar exports.
- [Browser-like project tabs](project-tabs.md) — one tab per project with
  snapshot-based switching, plus the new-tab and close affordances.
- [Tabbed settings and the shared tab strip](tabbed-settings.md) — the one
  browser-style strip behind project tabs and the Preferences sections: dock
  edge (left default for settings), overflow menu, reorder, pinning, grouping,
  four tab searches with regex builders, two bulk-close actions, per-surface
  persistence, and orientation-aware tablist accessibility.
- [External editor](external-editor.md) — configurable "Open in External
  Editor" for the current project folder, with editor auto-detection.
- [Config profiles & full-data backup](config-profiles-backup.md) — export the
  entire data directory (secrets included, behind a slide-to-confirm gate),
  import it on another PC as a new profile, keep unlimited profiles, and give
  each one local Git-backed snapshot history.
- [Export everything, in every format](export-everything.md) — one shared
  MD3 Export dialog for version history, preferences, presets, the object
  list and print statistics: JSON/JSONL/YAML/TOML/XML/CSV/TSV/Markdown/HTML
  with lossless/lossy badges and exact loss reasons, UTF-8 + LF/CRLF headers,
  ZIP (miniz) or 7z (installed 7-Zip, full option set incl. AES-256 and
  encrypted headers).
- [Preferences auto-history](preferences-history.md) — every settings change
  commits BambuStudio.conf into an isolated local Git repo (debounced,
  deduped), with a browser and restore-beside-the-live-file semantics.
- [Device fan motion](fan-motion.md) — independent telemetry-driven part and
  auxiliary fan previews with distinct input and command feedback.

- [Print preparation and workflow navigation](print-preparation.md): observational
  print review, existing explicit output and confirmation paths, and preserved
  workspace destinations. Native rendered verification remains pending.

## Studio Atlas workflow presentation

- [Prepare inspector and lists](../design-system/prepare-inspector-atlas.md): native preparation controls and measured source geometry.
- [Print setup presentation](print-setup-atlas.md): printer selection, review and existing send/export routes, including reopen lifecycle corrections.
- [Workspace composition](../design-system/workspace-atlas.md): Overview, Files, Checklist, Notes and Calendar layout with preserved data behavior.

These are source implementation receipts. Native rendering, supported-size interaction and the complete language/theme/scale matrix remain unverified where the articles say so.

## Postman collections

Not applicable. These are desktop workspace features with no HTTP or API
surface, so no Postman collection is provided for this category.
