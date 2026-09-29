# Native controls on the kit

A few native Windows controls were still on surfaces people use: the tip of a disabled button, the
web pages' notice bar, and the Workspace panel's tabs, lists, check list and month calendar. They
drew the Windows look in the system font, whatever the theme. Each now has a kit counterpart.

## What replaced what

| Native | Kit | Where |
| --- | --- | --- |
| `wxTipWindow` | `ButtonDisabledTip`: the Material plain tooltip | A disabled kit button's tip, which the system does not show for a disabled window |
| `wxInfoBar` | `MD3InfoBanner`: a Material banner | The notice above the web pages when a cloud page fails to load, with Retry |
| `wxNotebook` | `TextTabbar` over a `wxSimplebook` | The Workspace panel's sections |
| `wxListCtrl` | `wxDataViewListCtrl` in the Material table style | The Workspace panel's member list and its calendar agenda |
| `wxCheckListBox` | The kit `ListBox` with check boxes | The Workspace checklist |
| `wxCalendarCtrl` | `wxGenericCalendarCtrl` in the Material colours | The Workspace calendar |

## How

- **Disabled button tip.** A disabled window gets no system tooltip, so the kit Button shows its tip
  in a popup of its own. It is the Material plain tooltip, like every other tooltip: InverseSurface
  behind InverseOn text in the kit's small font, 8 x 4 DIP of padding and small rounded corners on
  Windows 11. It opens under the pointer, stays on that screen, and never takes the pointer or the
  focus.
- **Web banner.** `MD3InfoBanner` is a SurfaceContainerHigh strip with the status glyph (Warning in
  the Error role, Info in Primary), the message in the kit body face, a kit text button for the
  action and a close button, over an OutlineVariant edge. It keeps what the web panel used:
  `ShowMessage()`, `Dismiss()`, and an action that sends `wxEVT_BUTTON` with its id to the panel.
- **Workspace sections.** The kit `TextTabbar` switches a `wxSimplebook`. The tab bar now uses the
  Material roles everywhere it appears (the Workspace panel and the preset comparison): the surface
  it sits on, an OutlineVariant divider, and the active tab's label and indicator in Primary.
- **Tables.** `md3_style_data_view()` in `wxExtensions` gives a `wxDataViewCtrl` the kit body face
  in OnSurface on SurfaceContainerLowest, 32 DIP rows with a SurfaceContainerLow stripe on every
  other row, and a header in the kit's small title face and OnSurfaceVariant. The Workspace lists,
  Config profiles, Export, Version history and the notification centre use it.
- **Check list.** `ListBox::EnableChecks()` draws a Material check box glyph at the start of every
  row. A click on the glyph or the Space key toggles it and sends `wxEVT_CHECKLISTBOX` with the row,
  as the native check list did; a click elsewhere on the row selects it, for Edit, Move up and Move
  down.
- **Calendar.** The generic calendar paints itself in the colours it is given: SurfaceContainerLowest
  with OnSurface days, the weekday header in OnSurfaceVariant and the selected day in Primary. With
  sequential month selection it draws its own month header with arrows, instead of a native choice
  and spin control.

## What stays native, and why

- The system file dialogs, which bring the user's places, previews and the Windows shell with them.
- Native classes that no build can show: the SLA archive import's file picker (the import has no
  menu entry), the check-list combo popup in `wxExtensions` (no caller), and the monitor base panel
  with its splitter (never constructed).

## Verification

- `node --test ui-md3/tests/native-controls.test.mjs` refuses a native tab control, report list,
  check list, month calendar, tip window or info bar anywhere in the GUI, and checks each kit
  replacement. It fails on the previous tree in all six cases.
- A released build has not been driven through these surfaces yet.
