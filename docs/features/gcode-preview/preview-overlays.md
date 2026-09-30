# Preview overlays: who owns which part of the canvas

The sliced **Preview** canvas draws several ImGui overlays on top of the toolpaths: the plate strip on the left,
the status pill beside it, the legend dock on the right, the layer slider column, and the notification column in
the bottom right corner. They used to be placed independently, each from its own corner, so they overlapped:
notifications lay under the dock with their text cut, the slicing card hid under it, the status pill sat on the
plate strip, and two blocks inside the dock ended halfway across it. This article records the rules that now keep
them apart. Tracked in [issue #51](https://github.com/Ding-Ding-Projects/BambuStudio/issues/51).

## The rules

| Overlay | Rule | Source |
| --- | --- | --- |
| Notification column (toasts) | In Preview the column ends 16 px left of the expanded legend dock. With the dock folded it ends left of the layer slider column. Prepare and Assembly keep the 16 px corner anchor. | `NotificationManager.cpp`, `PopNotification::render` |
| Error banner and slicing progress card | The same right margin, clamped so the card never leaves the canvas. | `NotificationManager.cpp`, `SlicingProgressNotification.cpp` |
| Status pill (`Sliced · N layers`) | Starts 12 px right of the plate strip. It is drawn only when it ends before the dock; on a canvas too narrow for it, it is left out. | `BaseRenderer.cpp`, `render_legend` |
| All Plates Stats tile | The glyph and label are painted above the slice-state wash in opaque roles, so the wash dims only the tile background. | `GLCanvas3D.cpp`, `_render_imgui_select_plate_toolbar` |
| View-mode combo (`Summary`, `More`) | Spans the dock. | `BaseRenderer.cpp`, `render_legend` |
| Time estimation card | Spans the dock, and grows past it only for a line wider than the dock. | `BaseRenderer.cpp`, `render_legend` |
| Ink grouping card (dual-nozzle printers) | Its height is the measured height of its content, not a fixed line count. | `BaseRenderer.cpp`, `render_legend_color_arr_recommen` |

## How the widths reach each other

- The legend reports its width through `BaseRenderer::get_legend_dock_width()`, which is 0 while the dock is
  folded, disabled or not drawn. Both renderers clear it at the start of each frame, so an early return can never
  leave last frame's dock behind.
- `GLCanvas3D::render` adds that width plus 16 px to the right margin it hands to the notification manager, only
  while the preview was drawn this frame. The All Plates statistics view draws no dock and adds nothing.
- The plate strip publishes its own width (`GLCanvas3D::get_select_plate_toolbar_width()`), always the wider
  variant that includes its scrollbar, so hovering the strip never moves the pill. With no strip it publishes 0 and
  the pill keeps the 16 px inset.
- Inside the dock one span (`dock_x0`, `dock_span`) is taken from the window content region, inset by the window
  border. The view-mode combo and the time estimation card both use it, which is the span the ink grouping card
  already filled.

## Narrow canvases

A canvas can be too narrow to hold a notification beside the dock. The gap is then handed back, never below the
16 px corner margin, so the card stays on the canvas, and it is lifted in front of the dock so its text and close
button stay reachable. It is never lifted while a popup is open: a menu the person just opened stays in front.

The same lift keeps a toast in front of the G-code listing that opens in this column while the moves slider is
dragged. The two share the column for as long as both are shown. This is a deliberate trade: a warning stays
readable, and the listing returns as soon as the toast is closed or times out.

## The All Plates Stats tile

The tile used to draw its label in the brand colour at 20% alpha and a pale baked glyph under the slice-state
wash, about 1.2:1 against the wash. Now:

- one white glyph asset is tinted with the role for the state: `OnSurfaceVariant` when not sliced, `Primary` when
  sliced, `Error` when slicing failed;
- the label is `OnSurface`, or `Primary` when sliced;
- the button keeps its hit area but paints no image (tint alpha 0); the glyph is drawn after the wash;
- while a Helio job disables the tile, the glyph is drawn at half alpha, as the button's own image was.

A check computes the contrast on the darkest wash band in both themes: at least 4.5:1 for the label and 3:1 for
the glyph.

## The ink grouping card

The card is an ImGui child window with a fixed height. The height was a guess: a number of lines that assumed the
sentence under the nozzle boxes fits on one line. In the 344 px dock the sentence wraps to two lines, so the card
was about 10 px short, grew a scrollbar, and cut the last link off. Bilingual mode stacks a second line under every
text and made it worse.

ImGui 1.83 cannot size a child window to its content, so the card now measures itself: the content bottom
(`CursorMaxPos`) of the card and of the two nozzle boxes is stored and used as the height on the next frame, with
one extra frame requested when a value changes. Nothing is measured while the card is scrolled out of view,
because ImGui skips its items then and the cursor does not advance.

## Related sidebar fixes from the same report

- **Objects list.** The list no longer has the native column header (a white strip with `Name` and blank cells)
  or the 1 px system frame. It is created with `wxDV_NO_HEADER | wxBORDER_NONE`; the kit Objects card is rows under
  a search field. `GUI_App::UpdateDVCDarkUI` now accepts a table without a header and adds the system frame only
  to a table that left its border at the default.
- **Plate settings dropdowns.** Plate type, print sequence and the two ink sequence dropdowns take the width of
  their row instead of a fixed 12 em face, so a wider sidebar shows more of a long value such as
  `Textured PEI Plate` or `Smooth PEI Plate / High Temp Plate`. At the default sidebar width the row leaves no
  more than those 12 em, which stay as the minimum, so a long plate name still ends in an ellipsis there. The
  dropdown list is measured again when its face changes width.

## What was not a defect

- The purple play button, timeline and layer slider are the Preview accent of the design system, by design. The
  Helio partner dialogs use the same scheme.
- Two items in the report came from an installed copy built in July: the empty dropdown segments of the Slice and
  Print buttons and the blank gap above the object manipulation card. Both were already fixed in current releases.

## Failure modes

- If the legend is drawn by a code path that does not set `m_legend_width`, notifications fall back to the slider
  column and can lie under the dock again. The check in `ui-md3/tests/preview-overlays.test.mjs` pins the two
  renderers that draw it.
- If the ink grouping card is never visible, its height stays on the old estimate until it is first drawn.

## Verification

- Source checks: `ui-md3/tests/preview-overlays.test.mjs` (notification geometry, pill, tile contrast, dock span,
  card sizing) and `ui-md3/tests/native-controls.test.mjs` (Objects list, plate dropdowns). Each new check was seen
  failing on the previous source and passing on the change.
- The application is built only by the hosted Windows workflow. Captures of the Preview tab after slicing, in
  English, Hong Kong Cantonese and bilingual mode, are recorded on the issue once a release carries the change.

No HTTP API is involved, so no Postman collection applies.
