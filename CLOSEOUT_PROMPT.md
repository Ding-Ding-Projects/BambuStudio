# Independent settings draft continuation

## Objective and current steering

Implement independent settings drafts on the Prepare and Preferences tab strips. The maintainer requested preservation-first closeout before implementation and verification finished. This document is a continuation handoff, not a completion claim.

## Implemented source

- `SettingsDraftStore.hpp/.cpp`: owned draft/baseline configurations, duplicate independence, target and baseline checks, versioned bounded persistence, credential exclusion, stored search and scroll state.
- `SettingsDraftPanel.hpp/.cpp`: detached typed `ConfigOptionsGroup` editor, searchable page/type picker, new and duplicate drafts, changed-draft close protection, guarded diff confirmation, detached Save as preset without selection, a supplementary conflict-checked Undo Apply action.
- Prepare and Preferences host wiring and CMake source registration.
- `SettingsDraftUndo.hpp/.cpp`, `UndoRedo.hpp`, and bounded Plater snapshot methods: generic snapshot attachment carrying all three complete edited configurations through the existing Undo/Redo timeline.
- Dedicated feature documentation, state inventory, and `settings_drafts_tests` target.

## Verified and unverified state

`git diff --check` passed. Backend tests were written but have not been compiled or run. No application build, runtime interaction, screenshots, installer verification, or release occurred in this lane. No push or default-branch integration occurred in this lane.

The undo child initially wrote its four bounded files into the primary checkout. The exact diff and new files were transferred to this isolated lane. The primary originals remain retained for parent-owned preservation and recovery. This lane did not reset or remove the primary changes. The parent has asked that the four copied undo-seam files remain unchanged pending its byte proof.

## Required repairs and next steps

- Replace Undo payload filename identity with the stable project-tab identity already used by draft creation. Verify Save As and project switching semantics.
- Include configuration attachments in Undo stack memory accounting. Review generation, snapshot association, stale-target handling, and complete Undo/Redo restoration with real interactions.
- Connect `LocalConfigHistory` recording for draft edits and pre/post Apply, and expose a safe draft restore callback. The adapter currently lives in the separate history lane.
- Verify duplicate activation. `TabStrip::AddTab(..., activate=true)` sets model activation but does not emit the host activation event.
- Verify active-draft close fallback and Preferences startup restoration. Current page closure and layout loading require runtime checks.
- Review empty/vector/unsupported typed option controls, option labels and searching, narrow layout, themes, localization, accessibility, and corrupt-persistence feedback.
- Handle unsupported removed-option deltas explicitly. The store can return `removed_keys`; the panel currently applies only the cloned delta.
- Compile the exact integrated candidate and run `settings_drafts_tests`, then complete the required built application checks before asserting success.
- Integrate documentation indexes, roadmap, README, handoff, issue evidence, and deployment records through the parent task.

## Preservation boundary

Work is preserved on `codex/bambu-draft-tabs`. The parent owns pushes, integration, release, and cleanup. Do not delete or reset the primary recovery files or this branch without complete preservation and ancestry evidence. No host power action, physical printer command, private-source publication, or purchase is authorized by this handoff.
