# Live charts

**Surface:** the **Live charts** button under the temperatures on the Device
page, File > Live charts, the command palette (`Ctrl+Shift+F`, type "Live
charts"), or the Every feature page → `LiveChartsPanel`
(`src/slic3r/GUI/LiveCharts/LiveChartsPanel.{hpp,cpp}`), a page in the Local
tools tab. The rules live in the wx-free `LiveSeries` module
(`src/slic3r/GUI/LiveCharts/LiveSeries.{hpp,cpp}`).

Design: [design/local-tools-2026-11](../../../design/local-tools-2026-11/README.md),
board Liveline.

The nozzle, bed and chamber temperatures of the printer selected on the Device
page, over time, exactly as the printer sends them.

## Where the readings come from

The Device page hands every update to a recorder, which keeps a reading only
when the printer pushed a new report. A reading is never invented: when the
printer stops reporting for more than 10 seconds the line breaks, and the gap
stays a gap. The chamber is charted once the printer reports a chamber
temperature; until then it says **no sensor reported**.

The readings are kept for this session only, per printer: up to 14,400 per
series, about 4 hours at one report a second. They are never written to disk
unless you save them.

## The page

- **Now:** the latest nozzle, bed and chamber readings with their targets, and
  how many readings arrived this session.
- **Series:** Nozzle, Bed or Chamber.
- **Time window:** 1 min, 5 min, 15 min or 1 h, ending now.
- **View:** **Line** (every reading, joined except across gaps) or **Candles**
  (one per minute: first, highest, lowest and last reading; up candles in the
  accent colour, down candles in the error colour, the one still forming drawn
  hollow).
- **Target line:** the target temperature as a dashed reference line.
- **Pause** (`Space`) freezes the chart; readings keep arriving and the state
  says how many. **Resume** catches up.
- The large value, the trend in words and the state of the feed: **Live**,
  **Waiting** (before the first reading), **Stalled** (no reading for more than
  10 seconds, with the time of the last one) or **Offline**.

The trend compares the latest reading with the one about a minute earlier:
**Holding near** the target when it is within 1.5 °C or 1 % of a target above
zero, otherwise **Rising** or **Falling** by more than half a degree, or
**Flat**, with the change and the seconds it took.

The value range eases toward new readings, and the newest point pulses gently.
With reduced motion (Preferences, Interface motion, or the Windows setting)
both snap instead; every value, trend and state stays the same.

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
reader hears the latest value and its trend every 10 seconds through the
chart's name. Right-click the chart, or press `Shift+F10`, for Pause updates,
Copy the value under the crosshair, Save shown readings as CSV and Explain this
chart.

## CSV

`time,series,value`, one row per reading shown, in time order, with the time in
UTC (ISO 8601 with milliseconds) and the value to one decimal.

## Checks

`tests/live_series.test.mjs` compiles `tests/live_series_test.cpp`: readings
refused when not finite or out of order, the window, gaps, the trend words,
candles, the feed state, the crosshair, the value range and the CSV. Its
negative build joins readings across a gap and must fail.

## Not done yet

AMS humidity and fan charts, momentum arrows and particles on new highs, and a
12-hour clock choice are designed but not built.
