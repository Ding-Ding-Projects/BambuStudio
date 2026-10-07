# Studio Atlas setup index

The configuration wizard index uses a low-container surface, a rounded primary
container for the active step, a high-container hover wash and semantic progress
markers. The existing step list, indentation, active/hover state and hitbox
advance remain authoritative. Text is centered using its measured extent, and
the existing deferred minimum-width calculation includes trailing content padding.
The existing project logo is retained. No labels, navigation routes, selection
semantics, page order, timers or configuration data change.

This unit changes only `ConfigWizardIndex::on_paint` in production source.
Its baseline is `54810146717bbf4d531a17f4b6c471bd3300d190`; the design contract is
`3c8fe2708`, `design/workflow-refresh/surface-contracts.json`, surface `wizard`.
Run `node --test tests/config_wizard_atlas_index.test.mjs` for four focused source
checks, including an intentional navigation-routing mutation rejected by the
preservation check. These checks do not compile or execute native drawing.

The live design flow and application launch remain prohibited for this task.
No screenshot, runtime layout receipt, full build, keyboard acceptance, native
contrast measurement or DPI/theme acceptance is claimed. Existing keyboard and
focus behavior is preserved rather than newly implemented. The integration owner
must verify the resulting native surface at the supported tuples. Keep this paint
unit separately reversible from setup behavior and calibration work.
