# Prepare sidebar tabs

The Prepare sidebar has three tabs to make printer materials, print settings and the plate object list easier to find in a narrow sidebar.

- **Ink:** printer, bed, filament and AMS controls. The filament search filters material rows and retains its regex builder.
- **Process:** settings search and the full print settings tree. Search results can jump to a setting without rebuilding the tree.
- **Objects:** object search, plate/object list and a manipulation card when a valid selection exists.

Select a tab with the pointer or keyboard. The strip starts on the left with a compact 128 DIP rail, can dock to any edge through its context menu and saves its arrangement. If the sidebar is too narrow for every tab, use the tab strip's overflow control. The selected section starts at the top of the existing scroll area. Switching sections keeps the current presets, filament rows, object model and selection in memory.

## Limits and failure handling

The Process settings tree can be wider than the default sidebar. The horizontal scrollbar remains available for controls that cannot reflow. The manipulation card appears only when a valid object selection and 3D view provide values. A tab switch does not change printer settings or plate data.

This source change has not yet passed the hosted build or packaged-binary layout check. The earlier user image has no verified binary identity. See [the design and verification handoff](../../../design/prepare-sidebar-tabs.md) for the required capture matrix.
