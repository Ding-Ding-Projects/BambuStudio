# Printer melodies

**File ▸ Melody creator…** composes a tune the printer plays, previews it on this computer, plays it on the connected printer, copies its G-code, puts it into the printer preset's start or end G-code and keeps it as a file. Bambu Lab printers play their start and finish jingles the same way, so the stock tunes are included as presets to start from.

## How a printer plays a melody

The stock start and end G-code templates play their jingles with lines such as

```
M17
M400 S1
M1006 S1
M1006 A0 B10 L100 C37 D10 M60 E37 F10 N60
M1006 A46 B10 L100 C43 D10 M70 E39 F10 N100
M1006 W
M18
```

Read against the stock tunes, every `M1006` line is one time step with three voices: `A`, `B` and `L` are the note, the duration in ticks and the volume (0 to 100) of the first voice, `C`, `D` and `M` of the second and `E`, `F` and `N` of the third. A note of 0 keeps that voice silent. `M1006 S1` starts a sequence and `M1006 W` waits for it to finish; the motors are enabled before (`M17`) and released after (`M18`), which suggests the sound comes from the motors. Nothing documents the command: the note numbering (MIDI-style, 69 is A4 at 440 Hz) and the tick length are inferred, so the tick length is a setting of the melody that only the preview uses. Adjust it until the preview has the same pace as the printer.

## The dialog

- **Name** and **Tick length (ms, preview only)**. The tick length is remembered between sessions.
- **Start from**: an empty melody, the stock Bambu Lab X1, A1, A1 mini and H2D start and finish tunes, or a C major scale.
- **Steps**: one row per time step with a note, a duration and a volume for each of the three voices. A note is written as a letter, an optional `#` or `b` and an octave, such as `C4` or `F#3`; an empty note is silence. A value that cannot be read is refused with the reason and the cell returns to its previous value.
- **Add step**, **Duplicate**, **Move up**, **Move down** and **Delete step** act on the selected row.
- **Quick notation** replaces every step with a single-voice tune typed as notes with an optional `/ticks` and `r` for a rest, for example `c4 e4/5 g4/20 r c5`.
- The status line shows the number of steps and the length at the current tick length, or the first problem with the melody.

Footer actions:

- **Preview** plays the melody on this computer with square waves. The timbre is not the printer's; it only shows the shape of the tune.
- **Play on printer** sends the M1006 block to the printer selected on the Device page through the same route the Device page uses for its own commands. It is refused while no printer is selected, the printer is not connected or it is printing, and a notification confirms the send.
- **Copy G-code** puts the block on the clipboard.
- **Add to start G-code** and **Add to end G-code** write the block, inside `;===== melody start =====` and `;===== melody end =====` markers, into the printer preset's start or end G-code. An earlier melody block is replaced, otherwise the block is appended. The usual custom G-code validation runs first, and the preset must be saved to keep the change.
- **Save** and **Open** keep melodies as JSON files, by default under the application data directory's `melodies` folder; the last folder is remembered.

A melody has at most 256 steps, notes up to 127, durations up to 255 ticks and volumes up to 100.

## Express print mode

[Express print mode](../windows/express-print-mode.md) skips the stock start sound. A melody added through the creator sits in its own marker block and is never skipped, because the user put it there.

## Files and tests

- `src/slic3r/GUI/Melody/MelodyModel.hpp`: the model, free of wxWidgets: the G-code block, the importer for the stock tunes and any M1006 block, note names, the quick notation, the JSON file format, the WAV renderer and the presets.
- `src/slic3r/GUI/Melody/MelodyCreatorDialog.cpp`: the Material dialog.
- `tests/melody_model_test.cpp` (229 assertions, run by `tests/melody_model.test.mjs`) and `ui-md3/tests/melody-creator-localization.test.mjs` (catalog registration, English and Cantonese entries, placeholders, preset titles, the printing guard and the File menu item).

Verified on this change: the g++ syntax check of the dialog and the main frame on Linux, the two tests above, the dialog header include check and the Cantonese catalog check. Not verified: an MSVC build, the dialog in the running application, the preview on Windows and a melody played on a physical printer. Whether the inferred parameter meaning is right can only be heard on a printer.
