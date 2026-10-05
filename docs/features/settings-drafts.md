# Independent settings drafts

The plus action on the Prepare and Preferences section strips opens a searchable picker. Existing pages remain views of the existing page. New Process, Ink-preset, and printer-preset entries create independent settings drafts from the currently selected preset. Duplicate copies the draft, including unapplied edits, into another independently owned configuration.

Draft fields use the existing typed configuration controls and their validation. Draft edits never replace the application's global preset bundle. Each draft retains its original baseline, source name, stable project-tab identity, and target preset identity. Deleting or renaming a source preset does not discard the draft's owned settings.

## Apply and Undo

Apply compares the live target with the complete original baseline and checks the project and target identity. The confirmation lists changed settings with their previous and proposed values. The target is checked again after confirmation because the dialog runs a nested event loop. A conflict leaves the live target unchanged.

One existing Undo/Redo snapshot is taken before Apply. Its optional configuration attachment owns complete Process, Ink, and printer edited configurations. Undo and Redo restore those configurations along with the existing model state. Attachments follow snapshot pruning and are checked against the project generation and preset identities before history changes. The draft editor also offers a narrowly scoped Undo Apply action, which refuses restoration if later settings or target changes would be overwritten.

An exception during the live change attempts to restore the complete previous edited configuration. No printer command or physical-device operation is involved.

## Save as preset

Save as preset writes a detached user preset and inserts it into the collection with selection disabled. It does not activate the preset or change the live edited configuration. Existing names are protected against replacement. Failure retains the draft for retry.

## Persistence and privacy

Drafts persist in the local application configuration under `settings_drafts`, separately for each host surface. The versioned store retains baseline and current settings, source identity, search text, and scroll position. The tab strip owns the order, grouping, pins, dock edge, and active-tab view state. Closing a changed draft requires explicit discard confirmation. Built-in section closes hide those views instead of destroying them.

Credential-like option keys are excluded from draft snapshots, persistence, comparisons, and deltas. Restore is transactional and bounded to 64 drafts, 16 MiB total, 4,096 options per snapshot, and 1 MiB per serialized option. Incompatible storage does not replace a valid existing in-memory store.

## Verification status

`settings_drafts_tests` covers owned-copy isolation, duplicate independence, project/preset/revision conflict rejection, live-change rejection, missing-source persistence, transactional corruption handling, and credential exclusion. This change requires a real build and runtime verification of typed controls, both host strips, close confirmation, persistence, Apply, Save as preset, and complete Undo/Redo. Source review or a passing backend test does not establish those runtime results.
