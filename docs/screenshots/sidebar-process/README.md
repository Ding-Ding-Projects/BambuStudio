# Prepare sidebar — process settings

Before/after captures of the Prepare sidebar's process-settings surfaces. All taken headlessly from
the real Release build at **846 px** frame width (the width this host's display forces, and the
narrowest supported case), via `.claude/skills/run-bambustudio/`.

Behaviour, rationale and the measurements behind these are documented in
[`docs/features/prepare/process-settings-sidebar.md`](../../features/prepare/process-settings-sidebar.md).

| File | What it shows |
| --- | --- |
| `before-sidebar-clipped.png` | Advanced mode at the 344 dip default. Value fields sliced off the right edge, tab strip cut mid-`Support`, preset name truncated to `0.20mm Standard ...`. No horizontal scrollbar existed, so none of it was reachable. |
| `after-sidebar-readable.png` | Same surface after the fix. Values with their `mm` units, the full `Quality / Strength / Support / ⋯` strip, the full preset name, and further sections (`Seam`, `Advanced`) in reach. |
| `before-header-starved.png` | The over-subscribed header row: **no `Process` title and no Compare-presets button**, `Advanced` wedged against the `Global` / `Objects` switch. The controls were not clipped, they were allocated zero width. |
| `after-header-intact.png` | The same row with the title present and the `Global` / `Objects` switch properly spaced. |
| `after-search-settings.png` | The settings search pill on the **Simple settings** bar, giving the full tree the same `OptionsSearcher` + regex builder the compact card has. |

> [!NOTE]
> The Object-manipulation card is intentionally **not** pictured in the after shots: it is now
> hidden until a selection makes its values real, which is the fix. `press.py controls` confirms it
> is absent from the live control list with nothing selected.
