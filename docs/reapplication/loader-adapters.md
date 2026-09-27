# Project loading adapters on the official source base

The source base is official Bambu Studio `f977235e6d736c4c0b650520ac5a5b72cbfe9244`.
The existing feature implementation was inspected at `c5df6199e1a83b1c94be12e999c0b322fded8730`.
This is a source reapplication record, not a runtime crash diagnosis or a successful build report.

## Central host changes

- `MainFrame.cpp` retains the project-tab, workspace-member, and history entry points from the existing feature source. A tab switch, new tab, or file open now stops if the outgoing dirty tab cannot be snapshotted. A rejected incoming load does not activate or add the requested tab. The host attempts to restore the outgoing document from its snapshot or file and reports whether restoration succeeded.
- Closing the active tab loads its replacement while the active tab still exists. A rejected replacement keeps the existing tab. An already open workspace member only navigates to the editor after its tab actually becomes active.
- `Plater.cpp` keeps the official 3MF loading path as the primary model loader while retaining the existing history extensions. Reentrant load requests are rejected before clearing assembly state. A scoped flag reset releases the loading state if an exception interrupts the operation. A load with no project data returns before success finalizers run.
- The caller receives a `load_succeeded` result only after the project and its finalizers load. The result is checked by file-tab and workspace-member entry points. A returned dialog code alone is not evidence that project data loaded.

## Verification still needed

The official and reapplication candidates must each open the same saved 3MF under an isolated profile. The reapplication candidate must also exercise rejected paths, snapshot failures, dirty-tab switching, workspace members, and close-tab replacement. This source record does not establish the cause of the installed-release crash. No local build, test, application launch, or capture was run for this group.
