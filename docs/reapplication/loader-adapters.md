# Project loading adapters on the official source base

The source base is official Bambu Studio `f977235e6d736c4c0b650520ac5a5b72cbfe9244`.
The existing feature implementation was inspected at `c5df6199e1a83b1c94be12e999c0b322fded8730`.
This is a source reapplication record, not a runtime crash diagnosis or a successful build report.

## Central host changes

- `MainFrame.cpp` retains the project-tab, workspace-member, and history entry points from the existing feature source. A tab switch, new tab, or file open now stops if the outgoing dirty tab cannot be snapshotted. A rejected incoming load does not activate or add the requested tab. The host attempts to restore the outgoing document from its snapshot or file and reports whether restoration succeeded.
- Closing the active tab loads its replacement while the active tab still exists. A rejected replacement keeps the existing tab. An already open workspace member only navigates to the editor after its tab actually becomes active.
- `Plater.cpp` keeps the official 3MF loading path as the primary model loader while retaining the existing history extensions. Reentrant load requests are rejected before clearing assembly state. A scoped flag reset releases the loading state if an exception interrupts the operation. A load with no project data returns before success finalizers run.
- The caller receives a `load_succeeded` result only after the project and its finalizers load. The result is checked by file-tab and workspace-member entry points. A returned dialog code alone is not evidence that project data loaded.
- On rollback from a private tab snapshot, the host resets the Plater filename to the tab's real path, or to an empty path for an Untitled tab. It restores the tab label and dirty indicator from the saved tab model. Whether the Plater's internal dirty state also survives this round trip remains a runtime verification question.

## Verification still needed

The official and reapplication candidates must each open the same saved 3MF under an isolated profile. The reapplication candidate must also exercise rejected paths, snapshot failures, dirty-tab switching, workspace members, and close-tab replacement. This source record does not establish the cause of the installed-release crash. No local build, test, application launch, or capture was run for this group.

The focused hosted regression fixture must cover two outgoing documents: a dirty named project with a known real `.3mf` save path, and a dirty Untitled project with no real save path. For each, attempt to open an invalid incoming project after a successful private snapshot. Verify that the original tab remains active with its label and dirty indicator, that its model is restored, and that <kbd>Ctrl+S</kbd> targets the real named file or opens Save As for Untitled. Repeat for switching and closing an active tab whose replacement cannot load. Capture the dialog and final save target from the built candidate; source inspection alone cannot prove them.
