# Drawing board

**Surface:** File > Drawing board, the command palette (`Ctrl+Shift+F`, type
"Drawing") and the Every feature page →
`src/slic3r/GUI/Drawing/DrawingPanel.{hpp,cpp}`. The rules live in the
wx-free `DrawingModel` (`src/slic3r/GUI/Drawing/DrawingModel.{hpp,cpp}`).

Draw on a picture of a plate: circle the part that needs a brim, mark where
supports touch, write a note for whoever prints it next. The board opens on the
plate you are working on, and each plate of the project keeps its own drawing.
Everything is saved on this computer as you draw and is never uploaded.

## Pens

| Key | Pen | How it draws |
| --- | --- | --- |
| 1 | Pencil | Thin and slightly see-through; faster strokes come out thinner |
| 2 | Pen | Solid ink; faster strokes come out thinner |
| 3 | Fineliner | One even width, whatever the speed |
| 4 | Marker | One even width with flat ends |
| 5 | Highlighter | Wide, flat and see-through. A stroke that crosses itself does not get darker, because the whole stroke fades as one |
| 6 | Brush | Wide and slightly see-through; faster strokes come out thinner |
| 7 | Fountain pen | Thick or thin by direction, like a nib held at an angle |
| E | Eraser | Removes only what it touches. Erasing through the middle of a stroke leaves two strokes |

Each pen remembers its own **Width** and **Opacity**. **Smoothing** (0 to 90
percent) steadies a shaky hand and applies to every pen. Hold **Shift** while
drawing to keep the stroke on the nearest of eight directions from where it
started, which gives straight lines and clean diagonals.

## Colours

The colour row starts with this project's filament colours, followed by the
last eight colours you picked. **Colour...** opens the full colour picker, and
a picked colour joins the row. Choosing a colour while the eraser is in hand
switches back to the Marker.

## Undo, versions and Clear

- **Undo** and **Redo** (`Ctrl+Z`, `Ctrl+Y`) step through the last 200 changes:
  strokes, erasing, bringing a version back and clearing.
- **Keep this version** stores the drawing as it is now. The last 20 versions
  are kept per plate, with the time and the number of strokes. Double-click a
  version, or select it and press **Bring this version back**; that can be
  undone too.
- **Clear...** removes every stroke only after the two-key confirmation, and
  keeps a version named "Before clearing" first.

## The plate picture

When the board opens on a plate for the first time it takes a picture of that
plate from the 3D view. **Capture the plate again** takes a new one after you
move or change objects; your strokes stay where they are. If the 3D view has
not been shown yet, the board says so: open Prepare once, then capture again.
**Another plate...** switches the board to a different plate of the project.

## Export

**Export...** saves the drawing in one of six forms:

| Choice | Result |
| --- | --- |
| PNG image | The plate picture with the drawing on top, at the picture's size |
| PNG at twice the size, PNG at three times the size | The same, scaled up, with the strokes drawn again at that size rather than enlarged |
| PNG on a transparent background | Only the strokes, everywhere else see-through |
| SVG with the plate capture | Vector strokes over the plate picture, which is embedded in the file |
| SVG of the strokes only | Only the vector strokes |

Exports show exactly what the board shows, erasing included.

## Keyboard

With the board focused (click it or move to it with `Tab`): `1` to `7` pick a
pen and `E` the eraser. The arrow keys move the keyboard pen, 12 pixels a step
or 2 with `Shift`; `Space` puts it down and lifts it, so the arrows draw while
it is down; `Esc` lifts it. A focus ring shows when the board has the keyboard.

## Where drawings are kept

In the data folder, under `drawings`:

| File | Holds |
| --- | --- |
| `plate-<n>.json` | The strokes and versions of plate `n`, written to `plate-<n>.json.partial` first and then moved into place |
| `plate-<n>.png` | The last picture taken of plate `n` |

Drawings are kept by plate number, so they stay with the plate number rather
than with one project file.

## Checks

`tests/drawing_model.test.mjs` compiles `tests/drawing_model_test.cpp`: the
pens and their keys, each width rule (even, by speed, by direction), smoothing,
the eight-direction lock, the eraser removing only what it touches and
splitting a stroke in two, undo and redo with their limit, saving and reading
back (junk skipped), colour parsing, and the SVG export's size, see-through
layers, flat ends and embedded plate picture. Its negative build lets the
eraser take away strokes it never touched and must fail.

## Not done yet

Pen pressure from a stylus is not read yet, so every pen behaves as if pressed
evenly. Drawings are not stored inside the project file.
