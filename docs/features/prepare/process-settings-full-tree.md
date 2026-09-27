# Every process setting shown, no Simple/Advanced filter

## Behavior

- The Prepare sidebar's Process section is the full settings tree from first launch. The compact
  "simple settings" card and its "Advanced settings" / "Simple settings" flip are gone.
- `GUI_App::get_mode()` answers advanced for every stored `user_mode` except `develop`, so no
  option is hidden behind a mode. The "Advance" label and mode switch are no longer built in the
  settings header or on any settings tab.
- A stored `sidebar_process_advanced` value from an older profile is ignored on purpose.
- The sidebar body is the one scroll surface. The tree is hosted at its full content height
  (`ParamsPanel::fit_page_to_content`) and the body's sizer is laid out over the *virtual* height
  (`update_sidebar_scroll_body` in `Plater.cpp`), so every option row, down to the last row of
  "Others", is reachable by scrolling the sidebar. Laying the sizer out over the visible client
  height instead (plain `Layout()`) crushed the tree against the window bottom while the scrollbar
  pointed at empty space; that is the defect this rule exists to prevent.
- In Objects mode the object list does not stretch. Its height follows its visible rows
  (`Sidebar::fit_object_list_height`: header + rows, clamped between 180 dp and 420 dp) and is
  refitted when a plate or object is expanded or collapsed and when objects are added or removed.

## Verification

- Contract: `ui-md3/tests/md3-conversion-contracts.test.mjs`, "every process setting is shown".
- Layout probe: the `sidebar-check` driver command (`LayoutProbe.cpp`) logs the body's client,
  virtual and content heights plus the bottom of the last stacked child, and answers `DEFECT` when
  the last child does not end at the virtual bottom or the virtual height is below the content
  height.
- Capture: `docs/screenshots/md3-everything/prepare-advanced--en-light-comfortable--after.png`
- Objects mode, scrolled to the end of the sidebar body (the whole Frequent page is in view and the
  probe verdict was `ok`): `docs/screenshots/md3-everything/prepare-objects-bottom--en-light-comfortable--after.png`
  (top of the same surface: `prepare-objects-top--en-light-comfortable--after.png`).
  (the same surface at the 1000 x 600 minimum is `prepare-advanced-minimum--...--after.png`).

## Related

- [Prepare sidebar search](../windows/sidebar-search.md)
- [Layout clipping inventory](../design-system/cheap-jor-inventory.md) (CJ-012 is this sidebar)
