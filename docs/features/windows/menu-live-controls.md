# Live controls in menus

**Surface:** right-click (or `Shift+F10`, or the Menu key) on a setting row in
the Process, Filament and Printer settings, on empty space in the Prepare 3D
view, or on an object →
`src/slic3r/GUI/Widgets/MD3Menu.{hpp,cpp}` and
`src/slic3r/GUI/SettingContextMenu.{hpp,cpp}`. The rules for the controls live
in the wx-free `src/slic3r/GUI/Widgets/MD3MenuLive.hpp`.

A menu row can carry a live control: a **switch**, a **choice**, a **slider**
or a **stepper**. Changing it applies the value at once and the menu stays
open, so several values can be set in one visit. Everything else about the
menu stays as described in [Material context menus](material-context-menus.md):
search, shortcuts, submenus, and the way focus returns.

## Using a live row

| With | Switch | Choice | Slider | Stepper |
| --- | --- | --- | --- | --- |
| Click the control | Turns it on or off | Picks that option | Click or drag along the track | `−` and `+` move one step |
| Click the label | Turns it on or off | Moves to the next option | Nothing | Nothing |
| `Space` | Turns it on or off | Moves to the next option, wrapping | One step up | One step up |
| `Left`, `Right` | Off, on | One option back or on, stopping at the ends | One step down or up | One step down or up |
| `Enter` | Turns it on or off, then closes the menu | Closes the menu | Closes the menu | Closes the menu |

A value never leaves its range: a stepper stops at the setting's limits and
the `−` or `+` at a limit is drawn dimmed. Decimal values move on a grid that
matches their last meaningful digit (0.2 moves by 0.01, 15 by 1, 200 by 10).

A screen reader hears the label, the kind of control and its value, for
example "Show labels, check box, checked" or "Wall loops, spin button, 3", and
hears the new value when it changes. The row's description says which keys
change it.

## On a setting

Right-click a setting's name, its value or the empty space beside it, or press
`Shift+F10` on it. The menu has:

- **The value itself** as a live control: a switch for on/off settings, a
  stepper for numbers (with the setting's own limits and unit), and a choice for
  short lists. A longer list opens as a submenu with the current option marked.
  Settings typed as text have no live control; the menu then starts with the
  setting's name.
- **Back to the preset value**, naming that value, when you changed it.
- **Back to the system value**, naming it, when the preset differs from the
  system preset it is based on.
- **Copy the setting key**, for example `wall_loops`.
- **Compare with other presets...**, which opens the preset comparison for
  this kind of preset.
- **Explain this** (`F1`): a card beside the row with what the setting does,
  its key, its range and unit, its value now and its value in the saved preset.
  `F1` on a setting's field opens the same card directly.

Every change goes through the setting's own field, exactly as typing into it
would: the modified marks, the undo arrows beside the setting and the preset's
changed state follow as usual.

Right-clicking the value of a setting typed into a text box (most numbers)
opens the text menu (Undo, Cut, Copy, Paste, Delete, Select all) with the
setting's rows under it.

## In the 3D view

Right-click empty space in Prepare, with nothing selected:

| Row | Control | The same as |
| --- | --- | --- |
| Camera | Choice: Perspective, Orthographic | View > Use Perspective View, Use Orthogonal View |
| Show 3D Navigator | Switch | View > Show 3D Navigator |
| Show labels | Switch | View > Show Labels by Layer (by Object when the plate prints object by object) |
| Show Overhang | Switch | View > Show Overhang |
| Focus | Switch | Preferences > Attention modes > Focus |

Right-click an object:

| Row | Control | What it does |
| --- | --- | --- |
| Copies | Stepper | How many copies of that object are on the plate; each step adds or removes one |
| Printable | Switch | Whether the object is printed; a copy that is not printable stays on the plate, greyed |
| View | Choice: Solid, Wire | Draws the object solid, or as the edges of its triangles. Only the view changes; nothing is saved in the project |
| Opacity | Slider, 10 % to 100 % in steps of 10 | See through the object to what is behind it. Only the view changes; nothing is saved in the project |
| Snap to grid | Switch | Dragging an object puts its centre on the grid. Hold `Alt` while dragging to move freely. The same as View > Snap to grid |
| Grid step | Stepper, in mm | How far apart the grid lines are that dragged objects snap to |

Right-click a plate: **Locked** is a switch; Arrange and orient leave a locked
plate as it is. The rules for snapping (`src/slic3r/GUI/ViewTools/SnapGrid`)
and the per-object look (`src/slic3r/GUI/ViewTools/ObjectViews`) are wx-free.

## In the Preview

Right-click the Preview:

| Row | Control | The same as |
| --- | --- | --- |
| Color scheme | Choice | The legend's scheme (Line type, Speed, Layer height, and the rest) |
| One switch per option | Switch | The legend's options (Travel, Retractions, Wipe, and the rest) |
| Legend | Switch | The legend with the color scheme, the options and the statistics |

## On a Preferences row

Right-click a row in Preferences: its switch or choice is on the menu and
changes the setting at once, as the row's own control does, followed by **Back
to the default** with the default value shown, **Copy the setting key** and
**Explain this** (`F1`).

## For code

`MD3::AppendSwitch`, `MD3::AppendChoice`, `MD3::AppendSlider` and
`MD3::AppendStepper` add a live row to any `wxMenu` with a getter and a setter.
The control lives in the `wxMenuItem` (`MD3::Menu::LiveMenuItem`), so a menu
built once and shown many times keeps it, and outside the Material menu a
switch still works as a check item. `MD3::AddTextMenuSection` lets a feature
add rows under a text field's edit menu.

## Checks

`tests/md3_menu_live.test.mjs` compiles `tests/md3_menu_live_test.cpp`: what
Left, Right, Space and a click do to each kind of control, values staying in
range and on their step grid, the slider track mapping, how values read aloud,
and the step chosen for a decimal setting. Its negative build lets a stepper or
slider leave its range and must fail. `ui-md3/tests/context-menus.test.mjs`
still holds every menu to the Material menu.

## Not done yet

Live rows in the object list's own menus are not added. The rows on an
object's menu, the Preview's menu and the Preferences rows were built on 9
November 2026 and have not yet been run on Windows; the sources are checked by
`tests/view_tools.test.mjs` (snapping and the object look, with a negative
build) and `tests/live_menu_startup.test.mjs` (values are read only when a menu
opens, never while the Plater is still being built).
