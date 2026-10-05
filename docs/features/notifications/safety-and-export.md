# Notification history

Deletion reviews existing IDs in the current selection and filter, then captures that exact set in the confirmation callback. Later filter, selection, and retention changes cannot add targets. Completion reports both deleted records and reviewed records already unavailable. Unrelated selection stays selected. The shared two-key confirmation is the only deletion route.

Exports snapshot selected matching records, or every match when nothing matching is selected. The common export dialog provides formats, loss descriptions, archive settings, destination handling and external-editor handoff. Each row retains ID, timestamp, numeric and named level, notification type, title, full text, dismissal state and time, seen state, action, filter description and total recorded count. IDs remain decimal strings to avoid unsigned 64-bit precision loss.

Verification: the standalone C++17 model test covers changed selection after review, retention removing reviewed records, unknown IDs and filtered complete export. The Node consumer contract includes a negative mutation that substitutes later selection and must fail. Native dialog operation, translations, scale/layout and per-click capture remain pending a complete wxWidgets build.

Run `pwsh -File tests/notification_history/run-native.ps1` for the standalone native proof and `node --test tests/notification_history/consumer_contract.test.mjs tests/changelog_completion/exporter.test.mjs` for the seven source/exporter checks. These do not compile or drive the native dialog.
