# Studio Atlas: project, preferences and setup composition

This visual-only unit follows `design/workflow-refresh.md` and the
`native-project-preferences-and-setup` scope. Its baseline is
`be5e1205dcdad8f372d2ba63f367dbd7977c29c4`. It changes six native source anchors,
without changing the embedded stylesheet unit or shared widget implementations.

## Surface inventory and changes

| Surface | Composition implemented in this unit | Preserved routes |
| --- | --- | --- |
| Project | Separate rounded, outlined destination card with wrapping Online projects and Workspace actions; working content remains below it | Existing web fallback, workspace opening, navigation and script-message handlers |
| Preferences | Separate rounded search/content card beside the existing dockable section strip; lowest-container reading surface; 16/24/16 DIP title/row/right gutters; wrapping footer with tonal export and lower-emphasis reset | Stable section IDs, saved docking/order/pins/groups, search index, draft surface, every option registration and callback |
| Schedules | One rounded rule-management card containing heading, explanation, timezone, search, rule list, wrapping actions, detail and status; `Head_20` title and `Body_13` supporting styles; filled Add, tonal Edit and secondary ordering/toggle/delete actions | Existing search, selection, enablement, ordering, confirmation, persistence and scheduler listener |
| Calibration | Distinct outer surface, low-container navigation and lowest-container device selection body; density-based outer/tab padding | Device selection, calibration mode IDs, page registration and polling |
| Calibration wizard | Lowest-container scroll body against the low-container step surround, separated by active-density padding | Centered page layout, both scroll axes, step routing and calibration engine |
| Configuration wizard | Shared `Head_20` current-step heading and semantic foreground; active-density heading gap; distinct scroll-content surface; filled Next/Finish, outlined Back and text Cancel | Page index, custom-printer data, materials selection, navigation, IDs, validation and finish/cancel handlers |

Preferences keeps Appearance, Schedules, General, User, 3D, Other, conditionally
available Developer Tools and Settings draft. Existing nested headings remain:
Model import, Mouse Settings, Import Settings, Project, Online Models, AI printer
watch, Developer Mode, file associations, Log and Host Setting. The title font
used by the existing search index is unchanged. No option or localization key is
added, removed or renamed.

Configuration pages retain printer/material selection and custom printer,
firmware, bed shape, diameters and temperature setup. The shared page heading
changes their heading presentation, not their values or page sequence.

## Interaction and motion

New container windows retain `wxTAB_TRAVERSAL`. Existing controls continue to own
focus, input, disabled-state rendering and reduced-motion-aware feedback. No new
timer, animation loop, opacity transition, hardware action or asynchronous route
is introduced. Project and Preferences action groups wrap without stretch spacers
that would compete with wrapping. The Preferences footer remains outside its
scrollable settings book, and schedule details remain inside the scroll body.

## Source verification

Run `node --test tests/native_preferences_atlas.test.mjs` from the repository root.
The seven focused checks preserve the exact binding, option registration, value
and identity-call token sequences and every literal across all six anchors.
They compare the unchanged project, schedule and calibration engine tails with
the baseline; check card ownership, wrapping and action hierarchy; and reject
deliberate callback replacement and missing keyboard traversal. The baseline
commit must be present locally. These checks do not compile or execute C++.

No application, browser, installer or printer was launched. No full build,
screenshot or layout receipt was produced. Native minimum-size, bilingual,
theme, scale, keyboard, focus-return and motion acceptance remain unverified.
The preferred live Material Designer flow was unavailable under the explicit
no-launch boundary; the checked-in specification is source design data only.

## Remaining implementation boundaries

This is not completion of every nested surface in these feature families.
`WorkspacePanel`, `ConfigWizardIndex`, `CalibrationWizardPage` and their child
page/component implementations are outside this unit. Calibration changes here
are shell and step-surround composition only. The schedule rule editor and its
individual conditional fields retain their existing composition. Preferences
specialized controls and reset-detail dialogs retain their current internal
layout. Smart-home and authentication components are not modified. Missing
canonical functionality stays separate from this visual work.

Keep this unit as a separately reversible visual commit. Reverting it must not
remove independent build, serialization, scheduling, calibration or printer
fixes. The integration owner supplies the later build and runtime receipts.

## Card lifecycle and nested search repair

The initial composition commit `8b0b5e6da19d0ce877a3c5eb770de6945a516e77`
had two defects. Explicit pixel radii disabled the shared card's default DPI
rescaling. The new schedule card also moved registered rows below the immediate
page sizer, while the search index still visited only immediate items.

The follow-up uses `StaticBox::SetDensity` without an earlier radius override
on all three new cards. `PreferencesSearchTraversal.hpp` is used by the real
Preferences index through wx sizer/window adapters. It descends only through
containers proven to contain a registered row, stops at exact registered sizer
or window identities, preserves unregistered immediate rows and baseline
visibility, and excludes structural cards from row visibility mutations. Thus a
filtered child cannot hide its own card. Existing parent-relative scrolling
already accumulates ancestor positions and requires no replacement.

From an MSVC developer shell, run
`node tests/native_preferences_behavior.test.mjs`. This compiles one temporary
non-window fixture outside the repository. It inserts the actual production
`build_search_index` and card radius method bodies, plus all three actual card
initialization statements. Small in-memory substitutes supply window/sizer
identity and DPI conversion; no native window or application is created.

The original commit fails both independent negative runs: append
`--baseline=8b0b5e6da19d0ce877a3c5eb770de6945a516e77 --case=rows` to observe
`nested card was indexed instead of its rows`, or use `--case=radius` to observe
`card initializer pinned radius`. The repaired fixture passes nine cases,
covering exact key/text matching, filtering and reset, hidden ancestors,
registered stop boundaries, unregistered compound rows, structural wrappers,
spacers, all three initializers at four DPI scales, density changes and explicit
override semantics. The seven earlier source checks also pass.

MSVC reports warning C4458 for the unchanged production `SetCornerRadius`
parameter hiding its member; it is not suppressed or presented as a new defect.
These results validate the isolated production logic, not native rendering,
keyboard dispatch, real DPI events or the complete application build.
