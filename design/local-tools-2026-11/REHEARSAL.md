# Rehearsal and review

## Filming

The design tool could not film its own boards, so every board was rendered in headless Chromium at 1440 px wide with the fonts this app ships (Roboto, Roboto Mono and Material Symbols Outlined from `resources/fonts`). [render/prep.py](render/prep.py) swaps the hosted font links for those local files, and [render/film.mjs](render/film.mjs) shoots each board at its full content height. The twelve frames in [frames](frames) are the final pass.

Before the review, an icon check compared every icon name on every board with the glyphs in the shipped `MaterialSymbolsOutlined.ttf`. Four names were missing from that font and were replaced: `restore` with `history`, `new_releases` with `campaign`, `3d_rotation` with `view_in_ar`, and `file_download` with `download`.

## Fresh-eye review

A reviewer that had not seen the build looked only at the twelve frames, with the Stageshow review brief and checklist. It returned 27 ranked findings. Every one that could be fixed in the design was fixed, and the boards were filmed again.

| Finding | Fix |
| --- | --- |
| Large empty areas on Servers, Extensions, Setup, InfoIcons, ContextMenu and Drawesome; uneven card bottoms | Servers was reflowed into full-width rows, with recent events beside the overview and a schedule card beside Properties. Rows now stretch to equal heights. Panels gained real content (weekly runs, quick links, related settings, recent colours). Boards now end with their content |
| A board one-third empty below its content (toolbar shapes) | Filmed at its real board height |
| Numbers that disagreed across boards (attached disks, running servers, extensions on, new features, inks) | One number for each, matching the board that owns the data |
| Player time and console log that contradicted each other; a lag warning beside 20.0 ticks per second | Log times, player times and the warning line made consistent |
| A run listed for an extension that was switched off | The run moved to yesterday, before the permission change |
| A truncated failure reason; mid-phrase wrapping in setting keys and labels | Shorter wording, wider label columns, labels that never shrink |
| Reset dialog that said 4 settings and listed 3 | Says 3 settings and names the 15 that already match |
| Live chart crosshair at the wrong time, a rising trend over a flat line, progress percent that did not match the layer | Crosshair placed at 00:43:18 on a matched time scale, trend reads Holding near 220 °C, layer 47 of 212 |
| Version count off by one in the drawing Clear dialog | Says the 2 earlier versions |
| Two drawing palettes with different colours; a size value touching the next label | One nine-colour palette; spacing and a value for opacity |
| Base64 option labels that read as belonging to the wrong control; a line-length stepper shown while One line was chosen | Each label grouped with its control; the width moved into a Custom width choice |
| Base64 image preview showing an icon and a hex dump | Shows the decoded image with its real size, colour type and signature; the data: URL now describes the same 256 × 256 image |
| Recent activity out of order, with a time that did not match the server uptime | Newest first, with times taken from the other boards |
| Tiny, hard to read language buttons in the title bar | English, 廣東話 and Both as readable text |
| Backup choices out of order, a confusing search icon, low-contrast segmented labels | Off, 2 min, 10 min; a search icon; brighter segmented labels and shortcut keys |
| A floating submenu, a loose shortcut table | Submenu aligned with its row; tighter rows |
| Highlighted row on the feature page with no explanation; wrapped subtitle; unbalanced columns | No unexplained highlight; one-line subtitle; columns balanced |
| Disk resize slider with no scale, a growth figure that looked impossible, a sentence that began in its heading, an unlabeled column, monospace labels | Proportional slider with smallest, current and new size marks and a size field; the disk growth worded as disk size; full sentences; an Action header; sans labels |
| Truncated plugin names, inconsistent status pills, a three-card row under a four-card row | Wider name column, Needs restart and Free pills in one style, four cards per row |
| Monospace big numbers with wide gaps | Big readouts use Roboto with tabular figures |

Not applicable to these static product boards, and skipped by the reviewer: motion and beat timing, cursor choreography, loop seams, the 390 px phone width, stage framing and bleed.

Could not be checked from frames: hover and focus states, the light theme, exact contrast ratios and glyph detail at 4x zoom. The native builds will be checked with real captures of the built app.
