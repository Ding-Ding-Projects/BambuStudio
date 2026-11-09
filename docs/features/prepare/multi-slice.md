# Multi-slice: one sliced file per ink

Multi-slice slices the current plate once for each ink you choose and saves one
sliced file (`.gcode.3mf`) per ink. Use it when the same model should be ready in
several materials, for example a PLA and an ABS version on the printer's USB
drive, without changing the ink and slicing again by hand for each one.

Open it from **File > Export > Multi-slice**, from **Every feature**, or from the
command palette (search for "Multi-slice").

## Using it

1. Put the model on the plate and set it up as usual.
2. Open **Multi-slice**. Tick two or more inks. The search field filters the list
   by ink name or type, and offers the regular-expression builder like every
   other search.
3. Check where the files go. When a USB drive is plugged in it is chosen
   automatically; otherwise the last folder you saved to is used, or the Documents
   folder when there is none yet. **Change folder** picks another one.
4. Select **Slice and save**. Each ink shows a tag: **Waiting**, **Slicing**,
   **File saved** or **Not saved**. A failed ink gives the reason on a line under
   its name.
5. When it finishes, **Show files** opens the folder. If it was a USB drive, the
   usual eject button appears.

The files are named after the project and the ink, for example
`bracket_Bambu_PLA_Basic_BBL_X1C.gcode.3mf` and `bracket_Bambu_ABS_BBL_X1C.gcode.3mf`.
Spaces and characters that some file systems refuse become `_`; a name already
used in the same run gets `(2)`, `(3)` and so on.

## The window

- The header carries the title and a close button. The window can be resized.
- Each ink is one row in a rounded list. Pointing at a row fills it lightly; a
  chosen ink keeps a tonal fill, so the choice shows at a glance. Click anywhere
  on the row, or use its check box with the keyboard. The ink type (PLA, ABS, ...)
  is a small tag after the name. The header above the list names the two columns,
  **INK** and **STATUS**.
- The status tags are neutral while waiting or stopped, tonal while slicing,
  green once the file is saved and red when it was not saved.
- Under the list, one line sums up the state: how many inks are chosen, or
  "Slicing 2 of 3" with a progress bar while it runs.
- **SAVE TO** shows the folder once, shortened in the middle when it is long. A
  green **USB drive** tag appears when the folder is on the printer's USB drive.
  **Change folder** sits next to it.
- The footer keeps **Show files** at the left after a file is saved, then **Close**
  and the one committing action, **Slice and save**, at the right. While it runs
  **Close** becomes the red **Stop**.

## What changes for each file

- For each file, **every ink slot** of the project uses the chosen ink. The slot
  colours stay as they were. Multi-slice is meant for single-material prints; a
  multi-colour project still gets one file per ink, with every slot in that ink.
- Only the **current plate** is sliced.
- Process and printer settings are not changed.
- When the run ends, however it ends, the inks the project had before are put
  back.

## How it makes sure each file has the right ink

A file is saved only after the plate's slice result has been marked out of date
after that ink was applied and then become valid again. A result from the
previous ink can therefore never be saved under the next ink's name. If the
chosen ink is the one already in use, the result is marked out of date anyway so
that file is sliced fresh.

Before a file is kept, Multi-slice opens it and reads back the ink its settings
were sliced with. A file sliced with any other ink is removed and that ink is
reported as **Not saved**, so a file never carries the wrong ink under an ink's
name.

If a slice does not start within 10 seconds, stops without a result, or the
plate cannot be printed with that ink (for example the ink does not fit the
printer, or the toolpath leaves the bed), that ink is reported as **Not saved**
and the run moves on to the next one.

## Stopping

**Stop**, the close button in the header, or the Escape key finishes the ink being
sliced and then stops. The inks that did not start are shown as **Stopped**, and the original inks are
put back.

## Limits

- At least two inks are needed; for one ink, use **Export plate sliced file**.
- Only inks that fit the current printer are listed.
- Files are saved locally or to a removable drive. Sending over the network to a
  printer is not part of this feature.
- While it runs, leave the plate alone: a change to the model during the run
  makes that ink's slice out of date again, and it is reported as not saved.

## Verification

- `tests/multi_slice_names_test.cpp` checks file names: unsafe characters,
  trailing dots, non-ASCII ink names and repeated names.
- The run is checked in the built application on a 20 mm cube with an X1 Carbon
  profile, PLA and ABS: two files, each read back with its own ink (PLA at
  220 °C, ABS at 270 °C).
