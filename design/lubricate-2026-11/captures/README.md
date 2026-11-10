# Captures from the built app

Taken on 2026-11-10 from `build\src\Release\bambu-studio.exe` built by `build.bat -BuildOnly`,
running on a hidden Windows desktop at 150 % display scale, dark theme, English, with the
Bambu Lab H2C 0.4 nozzle selected and no printer connected. Each one was looked at before it
was kept. The commit in each row is the source the app was built from.

| Capture | What it shows | Built from |
| --- | --- | --- |
| [process-tabs.png](process-tabs.png) | The Process page on first open: both rows of setting categories show, Support and Others included | `4c418ea3a` |
| [lubricate-mode-on.png](lubricate-mode-on.png) | Others page, Special mode: Lubricate mode under Express print mode, switched on and marked as changed | `a4f0de812` |
| [device-lubricate-button.png](device-lubricate-button.png) | Device page at 1700x1000: the Lubricate button under the jog grid, disabled because no printer is connected | `a4f0de812` (before the gap under the button was added) |
| [melody-picker.png](melody-picker.png) | Print dialog: Lubricate melody with the search field, its regex toggle and builder button, the themed list with Truck backing up chosen, and the note for lubricate mode off | `1b10b4887` |
| [process-after-change.png](process-after-change.png) | Others page after a setting changed: the preset reads as modified and no horizontal scroll bar appears under the settings | `1b10b4887` |
| [all-plates-stats-tile.png](all-plates-stats-tile.png) | Preview with two sliced plates: the All Plates Stats tile draws its icon above the label | `1b10b4887` |
| [melody-search.png](melody-search.png) | Plain search `tw`: Two-tone siren and Twinkle, Twinkle, Little Star | `4c418ea3a` |
| [melody-regex.png](melody-regex.png) | Regex search `^(Ode\|Fur)`: Ode to Joy and Fur Elise only | `4ff4dfaf2` |
| [melody-no-match.png](melody-no-match.png) | Plain search `banjo`: empty list and "No melody matches the search." | `4ff4dfaf2` |

Checked in the same run, without a picture: with lubricate mode on, slicing a cube for the H2C
wrote the routine straight after `G28 Z P0 T250` in the start G-code, with
`; lubricate melody: truck_backup` and one buzzer note before each bed segment.

## Not reached

- The regex builder popover: a background click on its button did not open it on the hidden
  desktop.
- The Lubricate confirmation and the button while printing: both need a connected printer.
- Light theme, Cantonese and bilingual layouts of these screens.

## Differences from the boards and open layout issues

Fixed and captured again on `1b10b4887`: the melody list now uses the app's own list with
rounded green rows, the search field's buttons no longer draw marks over its outline, a changed
setting no longer adds a horizontal scroll bar on the Process page, and the All Plates Stats
icon no longer overlaps its label.

Still open:

- At 1200x800 the Device page scrolls sideways. Its status panel asks for about 2100 px because
  the ink column sits to the right of the Control column; fixing it means reflowing that page,
  which is outside this change.
- A long preset name with the changed marker is cut at the right edge of its box.
