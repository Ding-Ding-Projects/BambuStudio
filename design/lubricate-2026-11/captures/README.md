# Captures from the built app

Taken on 2026-11-10 from `build\src\Release\bambu-studio.exe` built by `build.bat -BuildOnly`,
running on a hidden Windows desktop at 150 % display scale, dark theme, English, with the
Bambu Lab H2C 0.4 nozzle selected and no printer connected. Each one was looked at before it
was kept. The commit in each row is the source the app was built from.

| Capture | What it shows | Built from |
| --- | --- | --- |
| [process-tabs.png](process-tabs.png) | The Process page on first open: both rows of setting categories show, Support and Others included | `4c418ea3a` |
| [lubricate-mode-on.png](lubricate-mode-on.png) | Others page, Special mode: Lubricate mode under Express print mode, switched on and marked as changed | `a4f0de812` |
| [device-lubricate-button.png](device-lubricate-button.png) | Device page at 1200x800, scrolled down: the Control column stacked under the camera column, with the Lubricate button under the jog grid, disabled because no printer is connected | `f9caf773c` |
| [melody-picker.png](melody-picker.png) | Print dialog: Lubricate melody with the search field, its regex toggle and builder button, the themed list with Truck backing up chosen, and the note for lubricate mode off | `1b10b4887` |
| [process-after-change.png](process-after-change.png) | Others page after a setting changed: the preset reads as modified, its long name ends in an ellipsis, and no horizontal scroll bar appears under the settings | `f9caf773c` |
| [all-plates-stats-tile.png](all-plates-stats-tile.png) | Preview with two sliced plates: the All Plates Stats tile draws its icon above the label | `1b10b4887` |
| [print-lubricate-switch-on.png](print-lubricate-switch-on.png) | Print dialog on a fresh setup: Lubricate before printing is on by default | `cc4af01e0` |
| [print-lubricate-switch-off.png](print-lubricate-switch-off.png) | The same switch turned off: the note changes to the once-at-start wording and the plate is marked for slicing again | `cc4af01e0` |
| [melody-search.png](melody-search.png) | Plain search `tw`: Two-tone siren and Twinkle, Twinkle, Little Star | `4c418ea3a` |
| [melody-regex.png](melody-regex.png) | Regex search `^(Ode\|Fur)`: Ode to Joy and Fur Elise only | `4ff4dfaf2` |
| [device-lubricate-button-yue-light.png](device-lubricate-button-yue-light.png) | The same Device page in Cantonese with the light theme | `f9caf773c` |
| [melody-picker-yue-light.png](melody-picker-yue-light.png) | The print dialog melody list in Cantonese with the light theme | `f9caf773c` |
| [melody-regex-builder-yue-light.png](melody-regex-builder-yue-light.png) | The regex builder opened from the melody search field, in Cantonese with the light theme | `f9caf773c` |
| [melody-picker-bilingual.png](melody-picker-bilingual.png) | The print dialog melody list in bilingual mode: every row reads "English · Cantonese" | `961c4b9ed` |
| [melody-no-match.png](melody-no-match.png) | Plain search `banjo`: empty list and "No melody matches the search." | `4ff4dfaf2` |

Checked in the same run, without a picture: with lubricate mode on, slicing a cube for the H2C
wrote the routine straight after `G28 Z P0 T250` in the start G-code, with
`; lubricate melody: truck_backup` and one buzzer note before each bed segment.

## Not reached

- The Lubricate confirmation and the button while printing: both need a connected printer.
- The bilingual layout of the Device page, which was captured but not added here: it shows the second
  language only on selected items, like the rest of that page.

## Differences from the boards and open layout issues

Fixed and captured again on `1b10b4887`: the melody list now uses the app's own list with
rounded green rows, the search field's buttons no longer draw marks over its outline, a changed
setting no longer adds a horizontal scroll bar on the Process page, and the All Plates Stats
icon no longer overlaps its label.

Fixed on `f9caf773c`: the Device page stacks the Control column under the camera column when
the window cannot hold both side by side, so it no longer scrolls sideways at 1200x800, and a long
preset name ends in an ellipsis instead of being cut at the edge of its box.
