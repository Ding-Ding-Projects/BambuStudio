# Bulk filament actions

Batch operations across many physical filament slots at once, staged in one MD3 dialog and
applied as a single batch.

## Behavior

The **BulkFilamentDialog** (`src/slic3r/GUI/BulkFilamentDialog.{hpp,cpp}`) lists every
*physical* filament slot — mixed (virtual) filaments are excluded, exactly as
`Sidebar::collect_physical_filament_info()` filters them — as a row of checkbox + numbered
color swatch + preset name, under a **Select all** toggle (indeterminate when partially
checked). Four actions can be staged:

1. **Set preset…** — opens a single-choice picker over the filament presets that are
   currently visible *and* compatible (the same set the sidebar combos show). The staged
   choice displays as the preset alias but is applied by full preset name.
2. **Set color…** — opens the MD3 color picker (`MD3ColorPickerDialog`); the staged hex is
   applied to `filament_colour`, `filament_multi_colour`, and `filament_colour_type` (`"1"`,
   solid) in one cloned project-config patch.
3. **Delete selected filaments** — deletes every checked slot.
4. **Add N filaments** — an independent numeric spinner appending N new filaments using the
   next auto-assigned colors (`Plater::get_next_color_for_filament()`).

Nothing changes while the dialog is open. **Apply** (wxID_OK) commits the batch;
**Cancel** is a strict no-op. Apply is disabled until an action is actually staged (and,
for the per-slot actions, at least one row is checked). Results surface through the
existing sidebar refresh — no modal success popups.

## Entry point

The **Bulk** outlined button in the Filament section header of the Prepare sidebar,
next to **Sync AMS** (same MD3 header-button recipe: h30 pill, `Body_11`, Material
`Stack` glyph). It calls `Sidebar::bulk_filament_actions()` (`Plater.cpp`), declared in
`Plater.hpp` alongside the other filament methods. The header layout math
(`Sidebar::priv::adjust_filament_title_layout`) accounts for the extra button so nothing
squeezes at narrow sidebar widths.

## Guards

- **min-1 rule** — deletion stops as soon as only one filament remains; the last filament
  can never be bulk-deleted. The dialog states this next to the delete action.
- **max-32 rule** — total filaments are capped at `EnforcerBlockerType::ExtruderMax` (32).
  The add spinner's range is `1..remaining capacity`; at capacity the add row is disabled.
  `add_custom_filament()` re-checks the cap per iteration, and the loop also stops the
  moment the filament-combo count stops growing.
- **Descending delete order** — checked slots are deleted from the highest config index
  down, so earlier deletions never shift the indices of later ones.
- **Single-refresh batching** — preset and color changes across all checked slots share
  one refresh tail (project-dirty update, selection export, combo updates,
  `on_config_change`, dynamic filament list update, backup-manager change log, per-plate
  slice-state invalidation, and — for color changes — one auto flushing-volume
  recalculation), instead of refreshing per slot. Deletions reuse `delete_filament()`,
  which performs its own per-slot refresh.
- Apply order is preset → color → delete → add, so per-slot actions always operate on the
  indices captured when the dialog opened; deletions cannot invalidate them.
- The gcode-3mf guard (`is_new_project_in_gcode3mf()`) runs before the dialog opens, same
  as the other filament-mutating flows.

## Failure modes

- **No compatible presets** — the Set preset… button is disabled; color/delete/add still work.
- **Stale slot set** — the dialog snapshots slots at open. Config indices are re-validated
  against the snapshot on apply; out-of-range entries are skipped rather than misapplied.
- **Capacity race** — if filaments were added elsewhere while the dialog was open, the add
  loop stops as soon as `add_custom_filament()` refuses to grow the list.
- **Delete down to one** — remaining staged deletions are silently dropped once one
  filament remains (matching the sidebar's single-delete behavior).

## Security considerations

Purely local UI over the in-memory preset bundle and project config; no network, file, or
process access beyond the standard project-dirty/backup bookkeeping every filament edit
performs. Localized strings go through the standard `_L()` catalogs.

## Verification

1. Build and open Prepare with a multi-filament project; the **Bulk** button appears in the
   Filament header next to **Sync AMS**, and the header still lays out at the narrow
   (312/344 DIP) sidebar widths.
2. Check several slots, stage a preset, Apply: every checked combo shows the new preset;
   slice state is invalidated once; project marked dirty.
3. Stage a color: checked swatches all show the solid color (`filament_colour_type` = 1);
   flushing volumes recalculate once when auto-calc is set to "all".
4. Check all slots and stage delete: deletion stops with one filament remaining.
5. At 30/32 filaments, the add spinner caps at 2; at 32 the add row is disabled.
6. Cancel after staging: nothing changes.
7. Language modes: strings render in English, Cantonese (批量耗材操作 …), and bilingual
   fallback; `compile_translation.py` and `--check` both pass.
