# Native reliability verification for issue 56

The first interactive candidate was
`0a12cd0945b782fb28d02cfc0cd1c02ed1ee094b`. It ran on a named, off-screen
Windows desktop with synthetic local projects and profiles. Each image receipt
records the executable and application DLL SHA-256. A later source fix does
not change the result recorded for that candidate.

| Workflow | Observed on the initial candidate | Separate verification still needed |
| --- | --- | --- |
| Ink deletion and printer selection | Switching the Prepare preset P1S → X1 Carbon → P1S kept the cube on the plate. Confirming deletion of an added second ink slot crashed in `wxEvtHandler::ProcessEvent`. [Preset evidence](../screenshots/reliability-56/printer-preset-receipt.json) · [Crash evidence](../screenshots/reliability-56/ink-deletion-pre-fix-receipt.json) | Repeat the actual menu, both keys and full slide after deferred dispatch is built. Verify that only the second slot disappears and that the application remains responsive. |
| Independent settings drafts | New Process draft created one stored draft and selected its tab, but the editor stopped at the compatibility list. [Pre-fix evidence](../screenshots/reliability-56/draft-before-field-fix-receipt.json) | Repeat on the compound-editor repair. Verify editing, duplication, Apply, Undo, cancellation and the recorded discard from the running application. |
| Prepare scrolling | A native scroll-to-end moved the filament header 72 device pixels. Virtual height, minimum content height and last-child bottom all measured 982 pixels. [Scroll evidence](../screenshots/reliability-56/cube-and-scroll-receipt.json) | Physical wheel and keyboard input, and minimum-size multilingual layouts, are distinct from this programmatic native scroll. |
| Readable dates | The native date unit cases passed. Timeline displayed date values under the Commit heading and truncated the date at the default width. [Timeline image](../screenshots/reliability-56/timeline-before-heading-fix.png) | Inspect the corrected localized widths, initial headings, and calendar input in the rebuilt dialog. |
| Responsive import and simplification | A generated 1,035,360-triangle STL simplified to 18,170 triangles. Stop preserved the pre-existing objects; a subsequent cube imported. Restoring the original meshes returned the scene to exactly 2,070,744 facets. Seven of 240 message-pump samples reached the 250 ms timeout. [Measurements and images](../screenshots/reliability-56/large-import-cancel-receipt.json) | Repeat against the ownership-transfer build. Verify the progress notice clears on cancellation. These measurements do not establish a universal frame-time bound or archive-reader cancellation. |
| Quiet interaction defaults | Eight source checks and 12 negative mutations passed. The clean synthetic profile gave no evidence about every printer-specific startup path. | Exercise the applicable initial and repeated startup flows on the accepted build. |
| Camera playback and pan/zoom | The camera geometry executable and all 18 playback-policy assertions passed. No camera stream was used. | Exercise the connected camera, pause, resume, minimized/hidden playback, reconnect and pan/zoom. |
| Local project/settings/printer history | A real search changed two matches to zero and persisted the submitted query. Comparing two project revisions returned their actual IDs and different geometry counts. Confirming restore and its rollback both failed, while the normal reader loaded one cube from the identical self-contained archive. Expanded filters also reduced the version-list height to zero. [Search evidence](../screenshots/reliability-56/history-search-receipt.json) · [Compare/restore evidence](../screenshots/reliability-56/compare-restore-pre-fix-receipt.json) | Repeat both restore and rollback on the corrected reader. Verify selectable rows, graph, date range and reachable actions on the rebuilt surface. The ten synthetic printer-incident cases do not establish live printer telemetry. |

## Focused native results

The MSVC build ran the dropdown lifecycle cases (30 assertions in 15 cases),
settings drafts (26 in four), saved searches (147 in four), dates (14 in
three), import and simplification including ownership transfer (40 in seven),
and project history (368 in 17). The printer incident executable covered ten
ledger and persistence scenarios. The Prepare scroll and web/date checks
passed eight and three tests respectively.

The transfer case linked the actual modified model translation unit. Other
native service targets reused existing compiled dependencies; none of these
unit-test results stands in for driving the final application.

## Dense import fixture

Generate the benign 20 mm sphere locally with Python and NumPy:

```console
python tests/automation-fixtures/generate_dense_sphere.py dense-sphere.stl
```

The captured fixture was 51,768,084 bytes. Its SHA-256 was
`3184bf139a7645688fdd9cec8138f1b3a1097fdfa90b4e27a57f207618bbc598`.
The generator requires an unused destination. Keep the source file and
compare its digest after recovery. The receipt distinguishes simplification,
cancellation during a separate import, subsequent worker reuse, and original
geometry restoration.
