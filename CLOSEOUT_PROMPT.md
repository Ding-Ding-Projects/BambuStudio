# Native lifecycle continuation

## Objective and current state

Repair Prepare filament-removal ownership and printer-selection callback lifetime.
Work is isolated on `codex/bambu-native-lifecycle`, based on commit
`0c967a55786c07ef639a2cbefbe922b619c157d3`.

Implemented changes:

- Blocking Material menus leave the popup callback stack and nested event loop
  before command dispatch. Command menu/check state and temporary appearance
  actions are snapshotted, and dispatch uses surviving tracked targets.
- Dropdown selections snapshot identity, dismiss child/root surfaces, and reject
  delivery after owner destruction or structural item replacement. Measurement,
  bitmap and flag invalidation preserve valid selections.
- The visible printer card opens its hidden item model explicitly, including
  keyboard activation. Missing model variants and canceled preset changes stop
  subsequent configuration and plate mutations.
- Filament row menus resolve current configuration slots. Deletion confirms a
  fixed target, rejects changed slot lists/flags during confirmation, protects
  the last physical filament, and lets parent destruction own sibling teardown.

## Verification

The actual production dropdown dispatch, invalidation and combo-adapter bodies
compiled with the existing MSVC toolchain against deterministic lifecycle doubles.
Result: **16 assertions passed in 8 test cases**. Disabling the generation check
produced the expected stale-row failure before restoration. Supplementary source
contracts passed **7 tests**. `git diff --check` passed.

No full application compile, native wxWidgets interaction, screenshot evidence,
installer verification, release or deployment has been completed for this change.
The body-double tests establish callback behavior, not a platform popup verdict.
A read-only independent review found and corrected overly broad generation
invalidation. Remaining native backend behavior must be independently verified.

## Changed files

- `src/slic3r/GUI/Plater.cpp`
- `src/slic3r/GUI/PresetComboBoxes.cpp`
- `src/slic3r/GUI/Widgets/ComboBox.cpp` and `.hpp`
- `src/slic3r/GUI/Widgets/DropDown.cpp` and `.hpp`
- `src/slic3r/GUI/Widgets/MD3Menu.cpp` and `.hpp`
- `tests/sidebar_filament_combos/CMakeLists.txt`
- `tests/sidebar_filament_combos/dropdown_lifecycle_tests.cpp`
- `ui-md3/tests/native-lifecycle.test.mjs`
- `docs/features/prepare/native-lifecycle.md`
- `CLOSEOUT_PROMPT.md`

## Next safe steps

The coordinating task preserves this branch remotely, integrates it only under
its reviewed recovery plan, and runs the full native build against a fixed commit.
Run `dropdown_lifecycle_tests`, `md3_menu_tests` and
`sidebar_filament_combo_bounds_contract` through the configured native build.
Reproduce P1S/H2C switching with clean/modified presets, save/discard/cancel,
mouse/keyboard openings, rapid reselection and physical/mixed filament removal.
Collect native focus/capture/owned-window evidence and a UI-thread stack for any
remaining freeze. Preserve all unrelated work; do not infer a runtime root cause
from source or deterministic doubles alone.

The user requested immediate preservation and cleanup after the account showed
8% remaining. Implementation stopped. This record is a continuation handoff,
not a claim that the application repair, release or wider goal is complete.
