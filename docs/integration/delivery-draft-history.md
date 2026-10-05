# Draft and settings history delivery

This delivery completes the coupled source changes tracked by issue #56. It uses the approved fast-delivery acceptance: implementation and release delivery, with no tests, lint, static-analysis suites, runtime interaction, installer execution or screenshots performed in this lane.

## Delivered behavior

- Undo attachments use the active project tab's stable identifier, retaining the existing project-generation and preset-selection checks. Saving a project under a new filename does not change its settings identity.
- Settings attachments contribute a bounded storage estimate to the Undo memory total. Attachment sharing is counted once per stack, and eviction recomputes the total after removing attachment owners. The estimate includes option serialization and per-option overhead; it is not a heap profiler.
- Repeated draft activation retains the existing editor instead of rebuilding it twice. Apply handles removed keys, and Undo Apply takes a new snapshot and refreshes edited-preset state.
- Draft edits, normal preset saves and detached Save as preset record sanitized local settings snapshots. Saving a detached preset retains the live selection.
- History restore loads a matching open draft into its detached editor. A preset snapshot can restore only while its exact preset is selected. It remains edited configuration until the user saves it. Both paths record the restored state as a new local version.
- Settings history comparison lists added, changed and removed keys. Selection retention identifies the source store as well as the commit.
- Settings history readers reject an invalid payload atomically. Preference snapshots validate supported numeric and boolean values before changing any live configuration.

## Boundaries

Draft history excludes sensitive and device-identity options through the existing local-config-history allowlist. A closed draft or changed project/preset target must be reopened or matched before restore. History restoration never activates another preset or sends a printer command.

## Build and delivery status

Source implementation only. A native build and packaging remain the coordinating release lane's responsibility. No functional, runtime, visual or installer-execution result is claimed here. The coordinating lane must record its exact integrated source commit and release assets before issue closure.

## Production build continuation

The production compiler reported `SettingsDraftPanel.cpp` C2065 for the unsupported `wxEVT_SCROLLWIN` identifier. The draft scroll-position observer now binds the eight supported specific wxWidgets scroll-window events with its existing deferred callback and editor-generation check. Native build retry is pending in the coordinating lane; no tests or screenshots were run for this repair.
