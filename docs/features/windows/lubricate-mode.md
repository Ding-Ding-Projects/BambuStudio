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
2. Two cycles. Each cycle plays the whole melody once while the machine stands still (one
   `M1006 S1`, every note, one `M1006 W`, the same shape as the start tune in the Bambu Lab
   profiles), then moves the bed all the way down and back up at F1200 in two single moves.
3. The toolhead along the full length of X, the full length of Y and both diagonals at F12000,
   then `M400`.
4. The printer's own homing again. On the H2C that is the whole `M1009 Q1 L1` ... `M1009 Q1 L0`
   window from the start G-code, so the bed is lowered and the toolhead parked over the bed
   before Z homes, exactly as at the start.

On printers with two toolheads (H2D, H2C) the X and Y range is limited to the area every toolhead
can reach (`extruder_printable_area`), so neither toolhead runs into its travel limit. On the H2C
that keeps X between 30 and 320 mm.

## At the start of every print

The option is `lubricate_mode` on the process settings **Others** page, in the **Special mode**
group, under **Express print mode**. It is on by default and saved with the process preset. The
**Send print job** dialog shows it too, as **Lubricate before printing** above the melody list;
turning it on or off there writes the process preset and slices the plate again, so the G-code
sent to the printer always matches the switch. A plate sliced before it was turned on has no
routine until it is sliced again, and a file sliced in another slicer or started from the
printer screen never has one.

With it on, `insert_lubricate_routine()` places the routine straight after the last full homing
line of the start G-code: a `G28` line, never a `G28.<n>` calibration command and never one inside
an `M622`/`M623` firmware conditional. On the H2C that is `G28 Z P0 T250`, after the first homing
pass, so the routine runs on a homed machine. The H2C homes Z inside `M1009 Q1 L1` and
`M1009 Q1 L0`, a pair the printer maker does not document, so the routine waits until after the
closing line and homes again inside the same pair, as the start G-code does. A start G-code with no plain `G28` (the P2S and the
X2D) gets `G28 ; home before lubricate mode` in front of the routine.

## The melody

Each preset carries pitches and individual durations. The longer cadence notes in Twinkle and Ode to Joy are held for their scored time. Truck backing up keeps a silent interval between its beeps. The same score drives the start-only and lubrication sequences.

The process option `lubricate_melody` holds the tune. It is chosen in the **Send print job**
dialog, under the print options, in a list with a search field. The field takes plain text, or a
regular expression with the `.*` toggle, and opens the shared regex builder from its tune button;
it matches both the shown name and the English name. Choosing a different tune in single-plate setup saves the process preset and starts a fresh slice.
After a successful slice, print setup reopens for review. Press **Send** to submit. The
list shows whether or not a printer is connected.

| Melody | Notes |
| --- | --- |
| Truck backing up (default) | B with a rest after it, four times |
| Two-tone siren | A and E |
| Rising chirp | C, E, G, high C |
| Ode to Joy | the first phrase |
| Twinkle, Twinkle, Little Star | the first phrase |
| Fur Elise | the opening |
| Westminster chimes | the first quarter |
| Silent | no sound |

The notes use the buzzer note numbers the Bambu Lab start tunes use (about 48 to 61); rests are
queued as silence so the rhythm holds. With lubricate mode on, the tune plays in full before each
of the two cycles. With it off, the whole tune plays once before the start G-code on Bambu Lab
printers. Silent turns both off.

Earlier builds queued one note, with its own `M1006 S1` and `M1006 W`, before every 25 mm bed
segment and used note numbers about two octaves higher. On a real H2C that sounded like random
beeps and made the bed stop and start every segment; the routine above replaces it.

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
[design/lubricate-2026-11/captures](../../../design/lubricate-2026-11/captures/README.md). The
2026-11-23 rewrite was checked by compiling the routine on its own and reading its output for the
H2C start G-code. Not checked: a run of the rewrite on a physical printer.

## Related

- [Express print mode](express-print-mode.md): the other Special mode option, and the switch the
  melody list copies in the print dialog.
- [Printer melodies](../device/printer-melody.md): compose a tune of your own and play it on the
  printer.
