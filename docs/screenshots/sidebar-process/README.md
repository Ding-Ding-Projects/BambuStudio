# Prepare sidebar — process settings

Before/after captures of the Prepare sidebar's process-settings surfaces. The two "before" images
were taken headlessly from the real Release build at **846 px** frame width (the width that host's
display forced, and the narrowest supported case), via `.claude/skills/run-bambustudio/`, and
committed on 2026-07-30. The "after" images have been retaken since, so each row below names the
capture its file comes from. The README's
[screenshot provenance](../../../README.md#screenshot-provenance) table lists the same sources.

Behaviour, rationale and the measurements behind these are documented in
[`docs/features/prepare/process-settings-sidebar.md`](../../features/prepare/process-settings-sidebar.md).

| File | What it shows | Source |
| --- | --- | --- |
| `before-sidebar-clipped.png` | Advanced mode at the 344 dip default. Value fields sliced off the right edge, tab strip cut mid-`Support`, preset name truncated to `0.20mm Standard ...`. No horizontal scrollbar existed, so none of it was reachable. | 846 px Release build, before the fix; never retaken |
| `after-sidebar-readable.png` | The whole Prepare sidebar, open on its Ink section: the section rail, the Printer card with its plate type and nozzle fields, and the Ink card, all inside the panel. The process values are not in this view. | Release `md3-v231` (source `31410274b`), README screenshot workflow run [37875388464](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/37875388464), 2026-10-09 |
| `before-header-starved.png` | The over-subscribed header row: **no `Process` title and no Compare-presets button**, `Advanced` wedged against the `Global` / `Objects` switch. The controls were not clipped, they were allocated zero width. | 846 px Release build, before the fix; never retaken |
| `after-header-intact.png` | A crop of the `PROCESS` section title only, not the header row; the recipe now aims at the whole header row, but its `md3-v231` retake was not done. | `scripts/md3/recapture.py` pass on build attempt 19 of `main` at `2cf53a936`, 2026-09-06 |
| `after-search-settings.png` | A crop of the **Search settings** field, which gives the full tree the same `OptionsSearcher` + regex builder the compact card has. | `scripts/md3/recapture.py` pass on build attempt 19 of `main` at `2cf53a936`, 2026-09-06 |

> [!NOTE]
> The Object-manipulation card is intentionally **not** pictured in the after shots: it is now
> hidden until a selection makes its values real, which is the fix. `press.py controls` confirms it
> is absent from the live control list with nothing selected.
