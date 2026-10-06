# Studio Atlas built-review queue

**Preparation only. Nothing in this queue has been driven, captured or accepted.**
The nine entries below correspond exactly to `remainingVisualCoverage` in
[implementation-scopes.json](implementation-scopes.json). Its `scopeFreeze` remains
in force: no product interface change is assigned before built review establishes
a concrete gap. Existing missing/planned/placeholder functional obligations remain
in the unchanged 1,204-row ledger. The 56 structural boards are not screenshots.

## Immutable target and source equivalence

The proposed target is the real local `build.bat /s` payload for
`a28944e3c14b2066ee63d14151c8aca23066d743`, whose observed invocation completed with
exit 0 at `2026-10-06T13:04:12Z`. The preparation source is preserved integration
`e9dd8e0a7c0200f212cb777a4bc7d5918f2ccecc`. Read-only Git comparison found **no
changes anywhere under `src/` or `resources/`** between these commits:

| Tree | Object at both commits |
| --- | --- |
| `src` | `097b02dc5f007fb9b99c4044452c4bf5a4546d91` |
| `resources` | `b7e164550cd2620bacdfbf8b9cc91dfa1bcf4736` |

Reproduce with `git diff --exit-code a28944e3c14b2066ee63d14151c8aca23066d743 e9dd8e0a7c0200f212cb777a4bc7d5918f2ccecc -- src resources` and
`git rev-parse <commit>:src <commit>:resources` for each commit. This proves relevant
UI source equality, not identical build environments or rendered output. The local
review scripts differ; use the reviewed integration version without relabelling the
target executable as an integration build.

Recorded payload SHA-256 values from the actual local build receipt:

| File relative to `install-dir` | SHA-256 |
| --- | --- |
| `bambu-studio.exe` | `4f71c93f61c071fd338884475f72eef49c4ff8dc0dfeceaa3ebc779b101d3750` |
| `BambuStudio.dll` | `9469e21f6c41cd5b36323aa68e93e30949d40f7a569878837fd7f9eb6535c339` |
| `automation/bambu-automation.exe` | `c960fad17e0bf90639d0d021cff7007b6977cd268b4b33a73b72f7e887ccad88` |
| `automation/build-identity.json` | `c84387865b1b41b649ee8b7cd256efb85c759c14aa920c96f9cb2cdca225478f` |

These are receipt values, not a claim that an actively staged payload remains
unchanged. Wait until all producers release it, then revalidate the actual files,
source and successful invocation receipt. Never inspect a moving build or installer
payload. If the target has changed, stop and rebind the queue to a reviewed candidate.
Keep the raw receipt, transcript, host paths and eventual unreviewed pixels private.

## Entry route, fixtures and common evidence

1. Resolve the pending authorization for visible-desktop execution and interaction
   first. This document supplies no authorization to launch, capture or navigate.
2. Use [local-native-review.py](../../scripts/md3/local-native-review.py) and its
   [local-build contract](../../docs/features/automation/local-native-review.md).
   Its default validates provenance only. The explicit future options are
   `--execute --desktop visible`; `--capture` is separate. Resolve the interpreter,
   Lowlevel CLI, producer and private receipt paths at runtime. Preserve the current
   fail-closed receipt checks and the hosted-only restrictions in
   `scripts/md3/drive-native-interface.py`.
3. The local route creates a fresh OS-temporary profile and handles only the initial
   shell. It does **not** automate this queue, resize windows or select matrix modes.
   Further driving needs separately reviewed interaction support or explicitly
   authorized, bounded native interaction through the existing Lowlevel route.
   Do not add guessed command-line flags, inject UI state, or replay old coordinates.
4. Use only disposable local data: a new project, a new Workspace, and the checked-in
   `tests/automation-fixtures/cube.stl` where specified. Create long names, notes and
   saved history through actual controls after that fixture work is authorized.
   No ready-made rich Workspace, calibration-result or AMS-device fixture is claimed.
5. Record each actual action, observed accessible target, pre/post semantic state,
   exact PID/HWND, native geometry and optional raw screenshot. Existing
   `send-layout-probe.py` can request the read-only dump; require its final `end`
   record and matching process/tag. A menu invocation hook is not native-input proof.
   OpenGL `canvas-png` evidence is distinct from native PrintWindow output. Do not
   infer embedded DOM or ImGui geometry from a native outer-window rectangle.
6. Existing `recapture.py` manifest labels such as `tab-bar-two-tabs`,
   `builder-popover`, `general-tab-scrolled`, `history-dialog` and `wizard-step-1`
   are discovery hints. That manifest records historical `2dcc26658` provenance;
   its six-destination coordinate fallbacks are not current proof. The state names
   below come from [manifest.json](manifest.json), not executable automation IDs.

## Tuple and sequencing contract

Start with one `en / light / comfortable / reduced-motion / 100%` initial-shell
observation. Normal client target is **1200 × 800 DIP**. Minimum review uses the
actual native limit from `GUI_App::get_min_size()` in `src/slic3r/GUI/GUI_App.cpp`:
`max(1000,76*em)` by `max(600,49*em)`. Observe native allocation and measured DPI;
do not assume a literal 1000 × 600 client at every font size or scale. If the normal
target is below the actual minimum, record that mismatch rather than force it.

For every reachable entry, first run its short normal-size path, then its narrow,
long-text path at the measured minimum. Expand to languages `en`, `yue_HK`,
`bilingual_en_yue_HK`; themes `light`, `dark`; display scales **100%, 125%, 150%,
200%**; densities `comfortable`, `compact`; and normal/reduced motion. This is
192 combinations per applicable state across two sizes, not 192 available fixtures.
Use native localized labels from the active build. Observe the actual scale and
motion policy, including system reduced motion; neither is established by a launch
flag. The current local driver fixes only the first profile tuple, so expansion is
pending interaction support and authorization. Never change host scale silently.

Order: **1, 2, 6, 7, 4 (local drafts), 9 (local model), 8 (local web entries),
3 and 5 (available account/device data), then 4's calibration results**. A missing
account or fixture blocks that entry only. Capture and inspect focus, wrapping,
scroll extent, popup placement and disabled-state contrast before advancing.

## Nine bounded walkthroughs

### 1. `shared-control-callers`

- **Entry:** Prepare's named `Slice options` and `Print options` buttons in
  `src/slic3r/GUI/MainFrame.cpp` open `SidePopup` from
  `src/slic3r/GUI/Widgets/SideMenuPopup.cpp`. Open each with an empty plate, traverse
  its enabled rows using native keyboard input, then dismiss with Escape. Do not
  invoke a slice/print/send row. Expected: the popup opens within the viewport,
  disabled rows stay inactive, and focus returns to the original options button.
- **States:** `menus/{submenu,disabled,keyboard,dismiss}`. Inspect label/support
  spacing and focus at normal/minimum tuples. `src/slic3r/GUI/Field.cpp` slider rows require an
  actually visible parameter control and known local preset before any adjustment;
  record its label/range first, then change and restore one value in the disposable
  project. No universal slider fixture is present. `src/slic3r/GUI/Widgets/Slider.cpp` owns that
  control; `src/slic3r/GUI/Widgets/TextTabbar.cpp` is additionally exercised by entry 7.

### 2. `shell-nested-fit`

- **Entry:** `File > New Project` (`MainFrame::new_project_tab`) creates a new local
  project tab. Add at most eight disposable tabs, reducing to the measured minimum
  until `More tabs` appears. If it does not appear within that bound, record overflow
  unobserved. Select one overflow row; expected: that existing tab becomes active
  without losing another tab or changing its identity.
- Open the tab context menu: `Pin tab`, then `Unpin tab`; `Move into group...`, then
  Cancel; `New group with this tab...` with a long synthetic name only when local
  fixture edits are authorized. Reopen and filter the group list with a nonmatching
  string; expected empty-state copy replaces rows, clearing restores them. Preview
  `Close tabs containing text...`, then Cancel without closing anything.
- **States/anchors:** `shell/{project-tabs,overflow,pinned-tabs,grouped-tabs,minimum-size}`;
  `src/slic3r/GUI/Widgets/TabStrip.cpp`, `src/slic3r/GUI/Widgets/TabStripDialogs.cpp` and
  `src/slic3r/GUI/Tabbook.cpp`. Nested dialogs need their own client/scroll measurements.

### 3. `specialized-continuations`

- **Entry:** printer selector `Bind with Pin Code` is handled by `PinCodePanel` in
  `src/slic3r/GUI/SelectMachinePop.cpp`; `GUI_App::popup_ping_bind_dialog()` constructs
  the page in `src/slic3r/GUI/BindDialog.cpp`. Signed-out use reaches a login notice,
  not the code-entry form. Record that state and stop; never sign in or enter codes
  just to fill the matrix. An already authorized account is required for form layout.
- Firmware release notes are constructed by `UpgradePanel.cpp` from existing device
  version descriptions, using `src/slic3r/GUI/ReleaseNote.cpp`. No device/version data
  means this state is unavailable. Do not request an update to manufacture content.
  Message-dialog wrapping can use entry 7's disposable unsaved-workspace prompt,
  always choosing its non-destructive cancel route (`src/slic3r/GUI/MsgDialog.cpp`).
- **States:** `farm/account`, `confirmations/{untouched,cancel}` and `monitor/firmware`.
  Expected: notice/form distinction, readable long content, visible cancel/focus,
  and no binding, update, login or destructive continuation after dismissal.

### 4. `dense-settings-subforms`

- **Entry:** `Edit > Preferences > Schedules`, then `Add rule`, opens the existing
  `ScheduleRuleDialog`. Inspect its default cross-midnight 20:00–07:00 draft and
  its date/time/source sections, enter a nonmatching panel search where available,
  and Cancel the draft. Expected: editor opens and closes with no new rule committed;
  empty/filter copy and scrolling remain readable. Do not enable a schedule or set
  an external endpoint. Inspect Preferences search/no-match and long setting labels.
- **States/anchors:** `preferences/{search,no-match,scheduled-settings}` and
  `schedules/{empty,rule-editor,cross-midnight}`; `src/slic3r/GUI/Preferences.cpp` and
  `src/slic3r/GUI/Schedule/ScheduledSettingsPanel.cpp`.
- `calibration/{results,save-result,validation}` belongs to
  `src/slic3r/GUI/CalibrationWizardSavePage.cpp`, constructed by
  `CalibrationWizard.cpp`. It requires a genuine supported calibration result and
  selected device/preset. No such fixture is supplied here. Do not run calibration,
  advance an artificial success state, or save a preset merely for coverage.

### 5. `device-nested-details`

- **Entry:** an already authorized Monitor device's AMS settings/material controls
  reach `src/slic3r/GUI/AMSSetting.cpp` and `src/slic3r/GUI/AMSMaterialsSetting.cpp` through
  `StatusPanel.cpp` (`UpdateByObj` and `Popup`). Inspect settings type/order,
  material search, long profile names and scroll limits without applying changes.
  Expected: selected device/AMS/tray identity remains constant after dismissal.
- Mapping is the real existing send/print confirmation's material-mapping popup in
  `src/slic3r/GUI/AmsMappingPopup.cpp`. It requires a ready local slice plus compatible
  device/tray data. Current `combined-print`/`combined-send` hosted scopes are route
  references only; their hosting restrictions remain. No local mapping fixture is
  supplied, and this queue authorizes neither slicing nor submission.
- **States:** `print/{mapping,external-spool,incompatible}`, `monitor/{connected,offline}`.
  Account, hardware and capability limits are explicit: offline empty Monitor cannot
  prove material/multicolor/nozzle layout; unavailable variants remain unobserved.

### 6. `reader-detail-variants`

- **Entry:** a visible native SearchField's `Regex builder` opens
  `src/slic3r/GUI/Widgets/RegexBuilderPopup.cpp`. Record the invoking query before
  opening and record the original Regex mode, case-sensitive, multiline and
  whole-word flag values before editing. Use synthetic sample text, enable Regex
  mode, then enter pattern `a`, `[`, and a valid nonmatching pattern; inspect
  matches, validation and reference rows. Toggle each other flag individually.
  Expected: the invoking field synchronizes after every pattern/flag edit,
  including invalid pattern text; validation does not roll back the query.
  Press Escape while the popover has focus: it dismisses without restoring the
  original query or flags. Explicitly restore the recorded query through the
  invoking field and restore every recorded flag through the builder controls.
  Observe the restored invoking query and reopen the builder to verify all four
  flags, then dismiss it without further edits. Source anchors:
  `SearchField::openBuilder()` in `src/slic3r/GUI/Widgets/SearchField.cpp` wires
  the live callbacks; `RegexBuilderPopup::onPatternEdited()` synchronizes before
  evaluation, and `Dismiss()`/`OnDismiss()` only dismiss the popover. Do not
  substitute a generated image.
- `File > Export > Export preferences` opens `src/slic3r/GUI/Export/ExportDialog.cpp`.
  On the fresh profile, change only format/archive selections to expose their
  explanatory fields, inspect disabled actions/tool-path disclosures, then close
  without exporting. `File > Version history` opens `src/slic3r/GUI/ProjectHistoryDialog.cpp`;
  an unsaved project may show an empty/unavailable state. Diff/restore preview needs
  separately prepared local saved revisions; never restore during this pass.
- `Help > What's new / Changelog` opens `src/slic3r/GUI/ChangelogDialog.cpp`. Filter
  to no-match, clear, open the date picker and dismiss. Expected: original rows return,
  date grid fits and reader focus is retained. States: `regex/{matches,invalid,no-match}`,
  `import-export/{format,options,validation}`, `history/{project,empty,diff}` and
  `changelog/{dates,search,no-match}`. No clipboard copy or file export is needed.

### 7. `workspace-inherited-details`

- **Entry:** Project's `Workspace` button in `src/slic3r/GUI/Project.cpp` reveals
  `src/slic3r/GUI/WorkspacePanel.cpp`. Visit its actual five tabs: Overview, Files,
  Checklist, Notes, Calendar. Expected: one selected page at a time, existing draft
  data retained, long labels and footer actions reachable at the measured minimum.
- In a new disposable workspace, use `Rename workspace` to replace a short title
  with a long title and back without resizing; do the same with a multiline Notes draft. Observe
  reflow and scroll extent. Open `Add project 3MF`, then Cancel its file picker.
  Checklist `Add` and Calendar `Add planned print` may expose existing input dialogs;
  cancel before saving. Populated rows, reorder and overlapping calendar entries
  need separately authorized local fixture creation, not invented probe rows.
- **States:** `project/{workspace,empty,files,notes,checklist,calendar-month,calendar-agenda}`.
  There is no History tab in this control. History belongs to entry 6; the board's
  `project/history` label is not permission to create a new feature or claim coverage.

### 8. `embedded-alternate-flows`

- **Entry:** the existing Home destination loads `resources/web/homepage3/home.html`
  through `src/slic3r/GUI/WebViewDialog.cpp`. Inspect signed-out/empty/offline content
  that actually occurs; do not claim remote content when unavailable. `Help > Setup
  Wizard` calls `GUI_App::ShowUserGuide()` and `WebGuideDialog.cpp`; use its actual
  enabled Next/Back controls and cancel before setup completion or plugin downloads.
- The existing create-filament action calls `Plater::priv::on_create_filament()`
  and `src/slic3r/GUI/CreateFilamentWebDialog.cpp`. Step 1's chosen mode routes Next
  to `step2.html`, `step2_copy.html` or `step2_type.html` via
  `resources/web/filament_create/step1.js`. Inspect only modes with valid local preset
  prerequisites, then Back/Cancel without creating a filament. No direct URL jump
  substitutes for that transition. Expected: the chosen mode and available preset
  rows agree, Back restores the documented step state, and empty rows remain readable.
- **States/anchors:** `home-web/{signed-out,empty,offline}`, `wizard/{region,printer-selection,filament-selection}`,
  `parameters/material`; `resources/web/homepage3/css/home.css`,
  `resources/web/guide/1/1.css`, `resources/web/filament_create/step2.css`.
  Native outer-window probes do not establish DOM overflow or bridge behavior.

### 9. `renderer-tool-interiors`

- **Entry:** `File > Import > Import 3MF/STL/STEP/SVG/OBJ/AMF` opens the native model
  picker. Import only `tests/automation-fixtures/cube.stl` after local fixture work
  is authorized. Select the object, open existing Move, Rotate and Scale tools one
  at a time, focus each axis input, inspect unit/label rows and dismiss without
  committing a transform. Expected: the selected object and transform values remain
  unchanged, while only the active tool's real inspector is visible.
- **States/anchors:** `prepare/{model-loaded,object-selection}` through
  `src/slic3r/GUI/GLCanvas3D.cpp`, `src/slic3r/GUI/Gizmos/GLGizmoBase.cpp` and
  `src/slic3r/GUI/Gizmos/GizmoObjectManipulation.cpp`. Add `preview/{legend,layer-range,statistics}`
  only when a genuine reviewed sliced fixture is available. This queue creates no
  slice. Other tool interiors require their own valid selection/model prerequisites.
- Inspect genuine native and OpenGL output separately. The existing native probe's
  `gl_item` rectangles locate toolbar items, not every ImGui input or tooltip. Missing
  renderer geometry instrumentation is a measurement gap, not evidence of good fit.

## Stop conditions and handoff

For each entry, return `not_started`, `unavailable` with its exact missing prerequisite,
or an action/evidence record tied to the actual build and tuple. Record unexpected
dialogs instead of guessing a target. A visual finding names the control, semantic
state, normal/minimum tuple, raw evidence and source owner before a bounded repair
is assigned. Missing interaction support, unavailable hardware and missing functional
engines are separate categories. Nothing here completes design parity, changes the
functional ledger, authorizes a new feature, or relaxes the current implementation freeze.
