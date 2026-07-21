# Dockable Prepare sidebar

## Behavior

The Prepare sidebar (printer, bed, and filament controls) can be docked to any edge of the
workspace: left, right, top, or bottom. Docking is driven by `wxAuiManager`. The user changes the
edge from a "Prepare panel position" control in Preferences and the sidebar re-docks live, without a
restart. The stored edge is authoritative over a restored layout perspective, so a right, top, or
bottom preference survives a layout reset, restart, and DPI change.

When the sidebar is docked to the top or bottom it becomes a full-width horizontal band whose height
is capped near 40% of the workspace (with a floor around 260 px) while the 3D canvas takes the
remaining vertical space; the sidebar keeps its own internal scrolling. Left and right docks keep the
vertical sidebar layout. The existing collapse behavior and the floatable-sidebar power-user option
are preserved.

## Configuration

- App-config key: `prepare_sidebar_dock`, one of `left`, `right`, `top`, `bottom`.
- Default: `left`. This deliberately overrides the design kit's right placement, per the requested
  default.
- User control: Preferences → "Prepare panel position" ("Dock the Prepare panel on the left, right,
  top, or bottom of the workspace.").

## Failure modes

- An unset or invalid `prepare_sidebar_dock` value is normalized to `left` at config load and again
  when the dock is applied.
- A previously floated sidebar is honored unless the user explicitly picks an edge (or the floatable
  option is disabled); an explicit edge pick re-docks the floating pane.
- When the dock orientation flips (vertical to horizontal or back), the pane's best size is re-seeded
  in the correct axis; otherwise the persisted size is kept so a user-resized sidebar survives
  restarts and DPI changes.

## Verification

- Implemented in `src/libslic3r/AppConfig.cpp` (default and validation), `src/slic3r/GUI/Plater.cpp`
  (`apply_sidebar_dock`), `src/slic3r/GUI/Plater.hpp`, and `src/slic3r/GUI/Preferences.cpp` (the
  control).
- Built locally in the Release configuration. This feature is part of local commit `8d727d49d`,
  which was not yet pushed to the hosted CI at the time of writing, so there is no hosted-run
  evidence for it yet.
