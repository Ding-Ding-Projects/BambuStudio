# Lubricate mode and print melodies

Lubricate mode runs the bed and the toolhead through their full travel so fresh grease spreads
along the rails and lead screws. It runs at the start of every print while the option is on, and
on demand from the Device page. While the bed moves, a Bambu Lab printer beeps like a truck
backing up, or plays another tune chosen in the print dialog. The chosen tune also plays at the
start of every print when lubricate mode is off, in every mode, Express print mode included.

The Bambu Lab H2C is the printer this was written for, and every example below uses its start
G-code.

## The routine

`lubricate_routine_gcode()` in `src/libslic3r/GCode/LubricateRoutine.cpp` writes the moves from the
printer's printable area and printable height, with a 5 mm margin on every side:

1. `G90`, then the bed to Z 5 mm.
2. Two cycles of the bed all the way down and back up at F1500, split into segments of at most
   25 mm. Before each segment it plays the next note of the melody on the buzzer (`M1006`).
3. The toolhead along the full length of X, the full length of Y and both diagonals at F12000.
4. `M400`, then the printer's own homing line again, because the bed and the toolhead ended
   somewhere else.

## At the start of every print

The option is `lubricate_mode` on the process settings **Others** page, in the **Special mode**
group, under **Express print mode**. It is off by default and saved with the process preset.

With it on, `insert_lubricate_routine()` places the routine straight after the last full homing
line of the start G-code: a `G28` line, never a `G28.<n>` calibration command and never one inside
an `M622`/`M623` firmware conditional. On the H2C that is `G28 Z P0 T250`, after the first homing
pass, so the routine runs on a homed machine. A start G-code with no plain `G28` (the P2S and the
X2D) gets `G28 ; home before lubricate mode` in front of the routine.

## The melody

The process option `lubricate_melody` holds the tune. It is chosen in the **Send print job**
dialog, under the print options, in a list with a search field. The field takes plain text, or a
regular expression with the `.*` toggle, and opens the shared regex builder from its tune button;
it matches both the shown name and the English name. Choosing a tune writes the process preset
the way the Express print mode switch does, so the plate is sliced again before it is sent. The
list shows whether or not a printer is connected.

| Melody | Notes |
| --- | --- |
| Truck backing up (default) | one high B, repeated |
| Two-tone siren | A and E |
| Rising chirp | C, E, G, high C |
| Ode to Joy | the first phrase |
| Twinkle, Twinkle, Little Star | the first phrase |
| Fur Elise | the opening |
| Westminster chimes | the first quarter |
| Silent | no sound |

With lubricate mode on, the tune plays one note per bed segment, in a loop, so the truck tune
beeps about once a second while the bed moves. With it off, the whole tune plays once before the
start G-code on Bambu Lab printers. Silent turns both off.

## From the Device page

The **Lubricate** button sits under the jog grid on the Device page. It asks first, because full
travel hits anything left on the plate, then sends `G28` and the routine to the printer, using the
printable area and height of the printer preset and the melody of the process preset. It is
disabled with the jog controls and does nothing while the printer is busy.

## Verification

Checked: a standalone test of the routine and the insertion rules, a survey of every Bambu Lab
start G-code template, the slice of a cube for the H2C in the built application (the routine lands
after `G28 Z P0 T250` with one note before each bed segment), and captures of the option, the
Device page button and the print dialog list in
[design/lubricate-2026-11/captures](../../../design/lubricate-2026-11/captures/README.md). Not
checked: a run on a physical printer. The timing of `M1006` against motion (whether `M1006 W`
holds the next move until the note ends) is the open question that only the printer can answer.

## Related

- [Express print mode](express-print-mode.md): the other Special mode option, and the switch the
  melody list copies in the print dialog.
- [Printer melodies](../device/printer-melody.md): compose a tune of your own and play it on the
  printer.
