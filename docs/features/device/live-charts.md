# Live charts

**Surface:** the **Live charts** button under the temperatures on the Device
page, File > Live charts, the command palette (`Ctrl+Shift+F`, type "Live
charts", "AMS humidity chart" or "Part cooling fan chart"), or the Every feature
page → `LiveChartsPanel`
(`src/slic3r/GUI/LiveCharts/LiveChartsPanel.{hpp,cpp}`), a page in the Local
tools tab. The rules live in the wx-free `LiveSeries` module
(`src/slic3r/GUI/LiveCharts/LiveSeries.{hpp,cpp}`).

Design: [design/local-tools-2026-11](../../../design/local-tools-2026-11/README.md),
board Liveline.

The nozzle, bed and chamber temperatures, the humidity inside each AMS that
reports one and the part cooling fan of the printer selected on the Device
page, over time, exactly as the printer sends them.

## Where the readings come from

From the moment the app starts, a recorder looks at the selected printer twice
a second, whichever page is open, and keeps a reading only when the printer
pushed a new report. The charts keep filling while you look at them in Local
tools, with the Device page hidden. A reading is never invented: when the
printer stops reporting for more than 10 seconds the line breaks, and the gap
stays a gap. The chamber is charted once the printer reports a chamber
temperature; until then it says **no sensor reported**.

- **AMS humidity:** one series per AMS unit that reports humidity. A unit that
  sends a percentage (AMS 2 Pro, AMS HT) is charted in percent; a unit that
  only sends a level (the first AMS) is charted as its level from 1 (wet) to 5
  (dry), and the two scales are never mixed. Units that report neither, such
  as the AMS Lite, are left out. A unit that can dry also says whether it is
  drying and how long is left.
- **Part cooling fan:** the percentage the Device page shows, from the fan
  speed the printer reports.

The readings are kept for this session only, per printer: up to 14,400 per
series, about 4 hours at one report a second. They are never written to disk
unless you save them.

## The page

- **Now:** the latest nozzle, bed and chamber readings with their targets, and
  how many readings arrived this session.
- **Series:** Nozzle, Bed, Chamber, AMS humidity or Part cooling fan. With two
  or more AMS units reporting humidity, **AMS unit** chooses which one.
- **Time window:** 1 min, 5 min, 15 min or 1 h, ending now.
- **View:** **Line** (every reading, joined except across gaps) or **Candles**
  (one per minute: first, highest, lowest and last reading; up candles in the
  accent colour, down candles in the error colour, the one still forming drawn
  hollow).
- **Pause** (`Space`) freezes the chart; readings keep arriving and the state
  says how many. **Resume** catches up.
- The large value, the trend in words, the momentum in words and the state of
  the feed: **Live**, **Waiting** (before the first reading), **Stalled** (no
  reading for more than 10 seconds, with the time of the last one and a dashed
  line where the readings stopped), **Offline**, or **Nothing to chart** when
  no AMS reports humidity or no chamber temperature came.
- **Small charts:** AMS humidity and the part cooling fan at a glance, over the
  same time window, with the value, the trend in a word and, for an AMS that
  can dry, whether it is drying. Click one, or focus it and press `Enter` or
  `Space`, to show it in the large chart.

The trend compares the latest reading with the one about a minute earlier:
**Holding near** the target when it is within 1.5 °C or 1 % of a target above
zero, otherwise **Rising** or **Falling** by more than half a unit, or
**Flat**, with the change and the seconds it took. Humidity and the fan have no
target, so they never hold.

The value range eases toward new readings, and the newest point pulses gently.
A percentage is drawn between 0 and 100 and a level between 1 and 5. With
reduced motion (Preferences, Interface motion, or the Windows setting) both
snap instead; every value, trend and state stays the same.

## Chart options

Saved for next time, in the app settings:

| Option | Default | What it does |
| --- | --- | --- |
| **Target line** | On | The target temperature as a dashed reference line |
| **Momentum arrow** | On | An arrow beside the newest point for the last ten seconds: up in the accent colour, down in the error colour, steady in the muted colour. Its slope follows the line on screen, never steeper than 60 degrees. The same momentum is in words beside the trend ("Going up, +0.52 °C a second") |
| **Particles on new highs** | Off | A short burst of particles when a reading beats every earlier one this session by at least half a unit, at most once every 5 seconds. With reduced motion no particles fly; a still **New high** note shows for 3 seconds instead |
| **Clock** | Windows' time format | **24 h** (13:05) or **12 h** (1:05 PM) on the time axis, under the crosshair and in the stalled note. Until you choose, it follows Windows' short time format |

**Preview reduced motion** shows the page as it is with reduced motion, for as
long as the switch is on; it is not saved. **Interface motion** opens
Preferences at the motion setting.

The momentum is the least-squares slope of the readings no more than ten
seconds older than the newest one: it needs two readings over at least two
seconds, and it is **up** or **down** when that slope moves the value by more
than 0.3 over the seconds covered, otherwise **steady**.

## Reading values

Point at the chart, or focus it (click it, or `Tab`) and use the keys:

| Keys | Action |
| --- | --- |
| `Left`, `Right` | Move the crosshair one reading |
| `Home`, `End` | First and last reading shown |
| `Esc` | Hide the crosshair |
| `Space` | Pause or resume |
| `Ctrl+C` | Copy the value under the crosshair |
| `Ctrl+S` | Save the shown readings as CSV |
| `F1` | This article |

The line under the chart shows the time and value under the crosshair. A screen
reader hears the latest value, its trend and its momentum every 10 seconds
through the chart's name, and each small chart's name says its title, value and
trend. Right-click the chart, or press `Shift+F10`, for Pause updates, Copy the
value under the crosshair, Save shown readings as CSV, Edit appearance
(including Animation and transition) and Explain this chart.

## CSV

`time,series,value`, one row per reading shown, in time order, with the time in
UTC (ISO 8601 with milliseconds) and the value to one decimal. The series name
says the unit for humidity (the AMS unit and `%` or the level) and for the fan
(`%`).

## Checks

`tests/live_series.test.mjs` compiles `tests/live_series_test.cpp`: readings
refused when not finite or out of order, the window, gaps, the trend words,
candles, the feed state, the crosshair, the value range and the CSV, then the
bounded range, the momentum and the arrow's angle, new highs with their margin
and cooldown, the particles (the same seed gives the same burst), the 12-hour
clock with midnight and noon, Windows' time formats and the saved options.
Three negative builds must fail: one joins readings across a gap, one bursts
on every new high with no cooldown and one shows midnight as 0 on a 12-hour
clock.

`tests/live_charts_wiring.test.mjs` checks the sources: sampling starts with
the app and stops when it closes, humidity is recorded per unit without mixing
the scales, the fan as the Device page shows it, the options are read and saved
with their keys, the chosen clock is used on the axis, under the crosshair and
in the stalled note, particles never fly with reduced motion, and the menu and
Every feature entries open what they say. Each check also fails on a broken
copy of the source.

## Not done yet

Nothing on the Liveline board is left to build. Fans other than the part
cooling fan (the aux and chamber fans) are not charted, because a printer
without them reports the same zero as one with them switched off.
