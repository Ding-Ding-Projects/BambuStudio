# Studio Atlas regex diagnostics and export details

## Reachable readers and scope

This source-only unit starts at `b5c42ce78a8e021509bdc504e5a40ab62d9cd8e4`. It implements the existing `regex` and `import-export` detail anatomy in `design/workflow-refresh/surface-contracts.json`, within the auxiliary reader scope.

`SearchField::openBuilder` creates the anchored `RegexBuilderPopup`. Its Build tab contains the editable pattern, live diagnostic and collapsible sample/results section. The Reference tab contains existing help and a local helper status. `evaluate()` owns validation, bounded matching, capture formatting and sample highlighting. `openCodeHelp()` owns the existing clipboard/launch result. These remain distinct content owners.

`ExportDialog::run` is called by object-list actions in `GUI_Factories.cpp` and `MainFrame.cpp`, print-statistics and configuration actions in `MainFrame.cpp`, configuration export in `Preferences.cpp`, history export in `ProjectHistoryDialog.cpp`, and preset export in `Tab.cpp`. Within the dialog, `update_format_details`, `update_archive_panel`, `update_hints` and `set_status` own fidelity/schema details, archive/tool availability, cost/encryption disclosure and operation status respectively. Serializers and file operations remain outside this composition change.

## Composition

- Regex validity and reference-helper status use persistent body-13 wrapping labels with zero minimum width and expanding sizer slots. Diagnostic content updates explicitly lay out and fit their owning scroll page, including a second pass for changed scrollbar allocation, while restoring its scroll position. They do not resize the popup or request focus.
- Sample and result editors expand to the existing test section's available width. Their initial heights are 96 and 144 DIP respectively. Both use readable monospaced size-13 text; results remain read-only. Existing sample limits, capture summaries, match highlighting and local editor scrolling remain unchanged.
- Export's fidelity notice uses a wrapping heading-14 label; loss/schema details use body-14 supporting text. The full translated loss notice remains visible in its own row.
- The 7-Zip executable/search-path detail now gets its own full-width wrapping row. The measured Locate action follows below with a density-derived gap, rather than consuming the diagnostic's horizontal space. Search visibility and archive selection continue to govern the same group.

No strings, catalogs, passwords, callbacks, validation decisions, serialization options, overwrite behavior or dispatch routes change. Existing Regex entrance/dismissal and reduced-motion behavior remain unchanged. No new animation is introduced. Other reference tables, chips, flags, export checkboxes, destination fields and the dialog shell remain outside this detail unit; this is not a claim that every nested control has been redesigned.

## Focused evidence and limits

Run `node --test ui-md3/tests/reader-details-atlas.test.mjs`: **9 checks passed**. The checked-in baseline fingerprints actual reader methods and event bindings from the stated starting revision, excluding construction methods. Only the two explicitly reviewed diagnostic-layout additions are normalized out of behavioral fingerprints. Negative mutations change password comparison and the regex match limit, remove notice wrapping, and remove the diagnostic content hook; each is rejected.

The diagnostic lifecycle check executes the actual status callback and reflow helper through a deterministic non-window adapter. Short-to-long-to-short diagnostic text changes the virtual extent at unchanged dimensions. The adapter uses simplified text metrics and scroll behavior; it is not wxWidgets execution and does not prove native focus or pixel geometry.

`git diff --check` and the changed-content public-boundary scan pass. No full build, application/browser launch, screenshot, installer execution or hardware operation was performed. Native compilation, actual control minimum sizes, long untranslated strings, English/Cantonese/bilingual rendering, 100/125/150/200% scale, light/dark contrast, keyboard traversal and reduced-motion rendering remain unverified. Those require the real built application and its supported capture route.
