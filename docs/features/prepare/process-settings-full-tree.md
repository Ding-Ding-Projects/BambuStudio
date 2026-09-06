# Every process setting shown, no Simple/Advanced filter

## Behavior

- The Prepare sidebar's Process section is the full settings tree from first launch. The compact
  "simple settings" card and its "Advanced settings" / "Simple settings" flip are gone.
- `GUI_App::get_mode()` answers advanced for every stored `user_mode` except `develop`, so no
  option is hidden behind a mode. The "Advance" label and mode switch are no longer built in the
  settings header or on any settings tab.
- A stored `sidebar_process_advanced` value from an older profile is ignored on purpose.

## Verification

- Contract: `ui-md3/tests/md3-conversion-contracts.test.mjs`, "every process setting is shown".
- Capture: `docs/screenshots/md3-everything/prepare-advanced--en-light-comfortable--after.png`
  (the same surface at the 1000 x 600 minimum is `prepare-advanced-minimum--...--after.png`).

## Related

- [Prepare sidebar search](../windows/sidebar-search.md)
- [Layout clipping inventory](../design-system/cheap-jor-inventory.md) (CJ-012 is this sidebar)
