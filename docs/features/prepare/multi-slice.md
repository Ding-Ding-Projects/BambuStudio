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
   automatically; otherwise the last folder you saved to is used. **Change folder**
   picks another one.
4. Select **Slice and save**. Each ink shows **Waiting**, **Slicing**, **Saved**
   or **Not saved** with the reason.
5. When it finishes, **Show files** opens the folder. If it was a USB drive, the
   usual eject button appears.

The files are named after the project and the ink, for example
`bracket_Bambu PLA Basic.gcode.3mf` and `bracket_Generic ABS.gcode.3mf`. Spaces
and characters that some file systems refuse become `_`; a name already used in
the same run gets `(2)`, `(3)` and so on.

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

If a slice does not start within 10 seconds, stops without a result, or the
plate cannot be printed with that ink (for example the ink does not fit the
printer, or the toolpath leaves the bed), that ink is reported as **Not saved**
and the run moves on to the next one.

## Stopping

**Stop**, or closing the window, finishes the ink being sliced and then stops.
The inks that did not start are shown as **Stopped**, and the original inks are
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
- The run itself is checked in the built application on a sample model with two
  inks.
