# Native print workflow state and implementation handoff

The whole-application visual contract is [Studio Atlas](workflow-refresh.md).
Its primary visual order is Prepare, Preview, Print, Monitor, with all auxiliary
destinations retained. Existing numeric page identities remain unchanged: append
the real Print page and project visual order through
`Notebook::SetWorkflowPages(prepare, preview, print, monitor)`. That projection
never changes callbacks, palette addressing, tab ownership or physical operations.
The light/dark source reference boards are illustrative, not production captures.

## Scope and source

This handoff covers the requested menus, dual-nozzle planning, slice and print continuation, fan and camera controls, LAN farm, Model Creator, project workspace, checklist, calendar, and portable file history. It describes production wxWidgets controls and the existing OpenGL/ImGui plate canvas. It does not treat a static reference as evidence that an interaction works.

The initial integration baseline is Bambu Studio `v02.08.04.57`, upstream source `f977235e6d736c4c0b650520ac5a5b72cbfe9244`. The fork's native design system, update feed, user presets, and Windows installer identity remain part of the product contract.

## Shared visual and interaction rules

- Use the native Material Design 3 color, shape, type, focus, and motion tokens already in this repository. Preserve the existing device teal role in print controls.
- New controls use the application's registered search, regex builder, keyboard navigation, focus restoration, localization, accessible names, and nonblocking notifications. A disabled action keeps a reason available through a visible label or tooltip.
- At the usual `1200 × 800` device-independent-pixel window, the plate remains the primary region. The minimum is the runtime `GUI_App::get_min_size()` result: at least `1000 × 600`, increased when `76 × em_unit` or `49 × em_unit` is larger. At the minimum, secondary detail scrolls within its panel rather than leaving its owning card.
- English, Cantonese, and bilingual text must all fit at the minimum and at 100%, 125%, 150%, and 200% display scaling. The control remains usable with light and dark themes and reduced-motion enabled.
- Keep API keys and LAN access codes out of rendered diagnostics, captures, exported projects, and design examples. A saved workspace never contains a credential.

## Screen and state inventory

| Surface | Required states | Native implementation destination | Evidence target |
| --- | --- | --- | --- |
| Filament object menu | enabled and disabled Edit, Delete, Decompose Color, Merge with; nested menu; search and regex; keyboard and dismissal | Existing popup construction and `MD3::PopupMenuBelow` in the plater | Popup bounds, submenu ownership, focus before and after |
| Grouping menu | existing modes, Prefer left, Prefer right, Custom; remembered preset; single-nozzle hidden choices | Filament grouping popup, print configuration, grouping solver | Three language modes and both nozzle capacities |
| Preparation action bar | Slice plate, Slice and print, slicing busy, error, stale result, print setup | Plater and MainFrame action controls | Normal and minimum width, double click, cancellation |
| Nozzle cards | selected material, per-material move, Swap groups, Swap and reslice, invalid mapping | Preparation grouping cards and plate-level mapping | Before and after assignments, unprintable explanation |
| Device print options | part and auxiliary fan idle, local pulse, awaiting telemetry, confirmed speed, zero and reduced motion | Status panel and fan popup | Real fan motion clip and still frames at zero and nonzero speed |
| Live camera | first eligible visit, connecting, playing, manual stop, manual play, reconnect, unavailable | Media player control | Each state tied to a selected printer identity |
| Device farm | account, discovered LAN, saved offline, paired, unavailable dual nozzle, six selected, partial transfer results | Device manager, picker, sending page, task views | No duplicates, visible offline row, explicit limitation |
| Model Creator | provider disconnected, credential editing, generating, preview, refine, error, cancellation, explicit add | Native creator panel beside Import and command palette | Four adapters, two renderer options, preserved last accepted preview |
| Workspace | overview, files, checklist, notes, calendar, empty and recovery states | Project subview beside online Project content | File identity, recent entry, stable member navigation |
| Calendar | month, agenda, slot editor, overlapping-slot warning, reminder due/snoozed/dismissed | Workspace planner and existing notifications | Date-only and timed timezone display at DST boundary |
| File history | revisions, lineages, current-only export, corrupt-history recovery, save interruption | Existing project history and 3MF save/load | Moved file, Save As, divergent copies, fresh-profile round trip |

## Key journeys

1. **Prepare and print.** Choose a printer preset and grouping mode. A new project inherits the preset's remembered mode; an imported 3MF keeps its explicit mappings. Select a plate, choose **Slice and print**, and wait for current slicing to succeed. The existing print setup opens only for that completed plate and generation. The user confirms with the final **Print** action.
2. **Correct grouping.** Open a nozzle card, move one material or choose **Swap groups**. **Swap** changes preparation assignments and invalidates the old slice. **Swap and reslice** also starts a new slice, then returns to print setup only if the current plate is still printable. Neither action changes the preset default or silently changes AMS slot selection.
3. **Monitor a printer.** The camera starts once on an eligible visit, printer switch, or reconnect. Manual Stop pauses the visit. The fan icons reflect confirmed speed; a local command gets a visibly different pending response until matching telemetry confirms it.
4. **Plan locally.** Create a workspace, add copied model files, write notes and checklist items, and place a timed print slot. A same-printer overlap warns without scheduling a print. Checklist and calendar exports are explicit. Reminders arrive through the application's existing nonblocking notification surface.
5. **Create a model.** Select a provider and model, describe the desired object, generate a bounded scene, preview an OpenSCAD or Blender result, refine it, then explicitly add a validated mesh to the plate. Failed revisions keep the last accepted preview. The direct API key stays in the operating-system credential vault.
6. **Carry history.** Save a 3MF or workspace with self-contained local revisions. Open it on a fresh profile, including after a move. Save As keeps the old file and creates a new document identity with inherited history. A corrupt history payload must not silently erase valid current geometry.

## Behavior and layout constraints

- Menus are reconstructed when opened, preserving current enable rules and explanatory copy. A submenu stays within the usable display bounds and returns focus to its invoking control on dismissal.
- At narrow width, the action bar keeps **Slice and print** visible as a distinct action. If any action must move to overflow, it retains its name and keyboard route.
- The nozzle cards show left and right as stable physical destinations. A single-nozzle printer does not present an unusable side choice.
- Fan controls distinguish press feedback from confirmed speed. Automatic updates use a smooth track and airflow ripple; local commands use a short pulse and a telemetry-confirmed settle. Reduced motion preserves state and speed information without continuous rotation.
- Offline saved LAN printers remain listed with unavailable state. A missing SDK capability, access code, address, or compatible nozzle mapping is explained before enqueueing a job.
- Workspace files copied from outside are identified by stable member IDs. A missing associated printer leaves the planned slot intact and visibly unresolved.
- Timed calendar entries store UTC and original timezone, while all-day deadlines remain date-only. The calendar never starts a print.

## Production capture matrix

For each state above, capture the normal and runtime minimum client sizes in English, Cantonese, and bilingual modes, light and dark themes, and 100%, 125%, 150%, and 200% scaling. Record the exact source commit, build receipt, executable SHA-256, screen and state, viewport, scale, theme, input route, accessibility name, privacy review, and image hash. Native controls require measured text and control rectangles; web-backed surfaces additionally require validated computed-layout probe receipts. Retain the real fan motion recording separately from still images.

No row in this matrix is considered verified from source code, a mockup, or a black capture. Capture and repair findings belong to the built artifact at the exact tuple that exposed them.

## Deliberate boundaries

Planning remains local. Print submission requires the user's final click. The Model Creator accepts structured scene data rather than generated executable scripts. Existing user projects, presets, credentials, and a running physical print are outside automatic migration or mutation. Live-provider and printer-hardware results are reported separately from simulated checks.
