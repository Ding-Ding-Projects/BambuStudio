# Atlas scrollbar owners in workflow and bulk callers

The integration scrollbar scanner identified raw wxWidgets scrollbar owners in six caller sites. This repair adopts the existing kit owners without changing their implementation or weakening the scanner.

| Caller | Existing owner replaced | Retained caller contract |
| --- | --- | --- |
| `Schedule/ScheduledSettingsPanel.cpp/.hpp` | Schedule editor scroll body and inherited settings panel become `MD3ScrolledWindow` | Existing parent, style flags, 12 DIP scroll rate, rule model, search, selection and schedule callbacks |
| `WorkflowPrintPanel.cpp/.hpp` | Inherited Print review panel becomes `MD3ScrolledWindow` | Existing parent, vertical scroll and traversal flags, 16 DIP rate, summary refresh, sizing and confirmed action routes |
| `WorkspacePanel.cpp` | Section-page factory constructs `MD3ScrolledWindow` | Existing section identity, parent, flags, 16 DIP rate, expanding layout, refresh/reflow and workspace data callbacks |
| `Bulk/BulkActionPreviewDialog.cpp` | Preview table constructs `MD3DataViewListCtrl` | Existing 220 DIP requested height, flags, columns, row model, selection and confirmation callbacks |
| `Bulk/BulkRenameDialog.cpp` | Rename preview table constructs `MD3DataViewListCtrl` | Existing 200 DIP requested height, flags, columns, rename-plan rows, selection and apply callbacks |

The source diff changes seven files: two inherited-owner headers and five implementation files. Only owner names and required kit includes change. Workspace's `wxScrolledWindow*` storage and both bulk dialogs' `wxDataViewListCtrl*` members remain unchanged. The kit owners publicly derive from those base classes and accept the same constructor arguments used here.

`MD3ScrolledWindow` retains the wx scroll helper and routes scrollbar state through `MD3ScrollBars`. Its existing keyboard, wheel and child-focus reveal behavior is inherited. `MD3DataViewListCtrl` keeps the data-view model and table behavior, routes scrollbar state to the same kit owner, and retains its existing arrow-key handling. This repair does not add a new scroll engine, replace models, set a reveal owner, change callback wiring or resize a caller. Kit implementations themselves are unchanged.

## Focused verification

The reported integration baseline at `a271602901c1b079a342dc9ea89edca57c5345c6` failed the raw scrolled-window and raw data-view scanner checks. The exact requested selection passed after this repair:

```powershell
node --test ui-md3/tests/native-controls.test.mjs ui-md3/tests/scrollbars.test.mjs ui-md3/tests/native-feature-delivery.test.mjs
```

Result: 29 checks passed, comprising 10 native-control checks, 10 scrollbar checks and nine feature-delivery checks. No scanner file or exclusion changed.

```powershell
node --test tests/native_shared_controls/atlas_scroll_owner_adoption.test.mjs tests/workspace/workspace_panel_atlas.test.mjs tests/native_preferences_atlas.test.mjs
```

Result: 24 checks passed, comprising nine owner-adoption checks, eight Workspace checks and seven preferences/Schedule checks. The new checks compare every changed source file with the exact baseline after undoing only the approved owner/include substitutions. Any changed constructor argument, callback, model operation, size, style flag or scroll-rate statement remains visible to that comparison. Negative checks reject a raw owner, changed control identifier and changed schedule callback. Base-class compatibility and unchanged pointer declarations are checked separately. Existing Workspace checks preserve 30 behavior methods, event bindings, page identities and content-layout lifecycle.

These are source checks. No full application build, launch, installer, hardware action or rendered capture ran. Native scrollbar drawing, wheel/page/keyboard motion, child-focus reveal, selection and layout at actual display scales remain unverified. The checked-in Atlas contract supplies design guidance, not runtime evidence.

## Reversal and scope

This separate owner-adoption commit can be reverted without changing kit implementations or data models. A revert restores the raw scrollbar owners and the scanner failures. Other control families, caller redesign work and the application-wide runtime parity matrix remain outside this repair.
