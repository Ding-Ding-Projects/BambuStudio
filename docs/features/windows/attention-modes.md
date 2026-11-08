# Attention modes

**Surface:** Preferences > Attention modes, the strip under the title bar, the
command palette (`Ctrl+Shift+F`, type "Attention") and the Every feature page →
`src/slic3r/GUI/AttentionModes/AttentionService.{hpp,cpp}`. The rules live in
the wx-free `AttentionModel` (`src/slic3r/GUI/AttentionModes/AttentionModel.{hpp,cpp}`).

Five interface accommodations. Each one is **off by default**, and each one
turns on and off on its own: none of them needs another, and there is no
switch for all of them. They change only how Bambu Studio looks and behaves for
you. Nothing is measured, scored, counted as a streak or shared, and none of it
is advice or a judgement about anybody.

| Mode | What it does |
| --- | --- |
| Focus | While something is selected in Prepare (and no tool such as painting is at work), everything else on the plate is drawn muted toward grey. Nothing is hidden: clear the selection, or press **Turn Focus off** in the strip, and everything is back |
| Low stimulation | Less motion: transitions settle at once, as with the reduce-motion setting, which it adds to and never undoes. No action sounds. No dim sum surprise at start-up |
| Time awareness | The strip under the title bar states how long this session has been open and how long since the last change, for example "Open 1 h 12 min · last change 4 min ago", or "no changes yet" |
| One thing at a time | The strip shows the one next step you typed ("Next: check the brim on plate 2"), with **Edit** and **Done**. It is kept across restarts until you mark it done |
| Momentum | When nothing has changed for a set time (15 or 30 minutes, 1 hour or 1 hour 30 minutes), one quiet notification says so plainly, for example "Nothing has changed here for 40 min." It speaks once per untouched stretch, and **Not now** keeps it quiet for the period you set (30 minutes to 4 hours), across restarts |

A change is anything that adds a step to undo: moving, adding or removing an
object, changing a setting on an object, and so on.

## The strip

The strip under the title bar appears only while Time awareness, One thing at a
time or Focus is on. It shows only what those modes need, updates every two
seconds (the times are in whole minutes), and its **tune** button opens
Preferences on Attention modes.

## School mode

School mode leaves the attention modes as they are: they are accommodations,
not presentation choices, so a school setup can turn them on or off like
anyone else.

## Stored values

| AppConfig key | Value |
| --- | --- |
| `attention_focus`, `attention_low_stimulation`, `attention_time_awareness`, `attention_one_thing`, `attention_momentum` | `true` or `false`; `false` unless you turned it on |
| `attention_momentum_minutes` | `15`, `30`, `60` or `90` |
| `attention_not_now_minutes` | `30`, `60`, `120` or `240` |
| `attention_next_action` | The next step, one line of at most 120 characters |
| `attention_snoozed_until` | When **Not now** ends, in seconds since 1970 |

## Checks

`tests/attention_model.test.mjs` compiles `tests/attention_model_test.cpp`: every
mode off by default and independent, saving and reading, the next step kept to
one line, the elapsed times, and Momentum speaking once per stretch and never
while Not now lasts. Its negative build lets Momentum repeat itself and must
fail.

## Not done yet

Focus outside the Prepare view (lists and Preferences rows) and a quieter
colour scheme for Low stimulation are not built.
