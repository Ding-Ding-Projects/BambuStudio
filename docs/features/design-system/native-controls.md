# Native controls on the kit

A few native Windows controls were still on surfaces people use: the tip of a disabled button, the
web pages' notice bar, the Workspace panel's tabs, lists, check list and month calendar, every group
box, the bed shape page chooser, the OK and Cancel buttons of three dialogs, and the scrollbar of
every scrolled page, panel, list, table, text box and HTML view. They drew the Windows look in the
system font, whatever the theme. Each now has a kit counterpart.

## What replaced what

| Native | Kit | Where |
| --- | --- | --- |
| `wxTipWindow` | `ButtonDisabledTip`: the Material plain tooltip | A disabled kit button's tip, which the system does not show for a disabled window |
| `wxInfoBar` | `MD3InfoBanner`: a Material banner | The notice above the web pages when a cloud page fails to load, with Retry |
| `wxNotebook` | `TextTabbar` over a `wxSimplebook` | The Workspace panel's sections |
| `wxListCtrl` | `wxDataViewListCtrl` in the Material table style | The Workspace panel's member list and its calendar agenda |
| `wxCheckListBox` | The kit `ListBox` with check boxes | The Workspace checklist |
| `wxCalendarCtrl` | `wxGenericCalendarCtrl` in the Material colours | The Workspace calendar |
| `wxStaticBox` (group box) | `MD3GroupBox`: a Material outline and title | Option groups outside the settings tabs, the bed shape dialog, the calibration wizard pages, Save preset, the unsaved changes comparison, the ink picker's preview |
| `wxChoicebook` | The kit `ComboBox` over a `wxSimplebook` | The bed shape dialog's shape |
| `CreateButtonSizer()`, `CreateStdDialogButtonSizer()` | Kit buttons with the standard ids | Bed shape, System info, the full comparison of an unsaved change |
| The Windows scrollbar of `wxScrolledWindow`, of the kit `ListBox`, of every table (`wxDataViewCtrl`, `wxDataViewListCtrl`), of every multi-line text box and of every HTML view (`wxHtmlWindow`) | `MD3ScrolledWindow`, the kit `ListBox`, `MD3DataViewCtrl`, `MD3DataViewListCtrl`, `TextAreaEditor` and `MD3HtmlWindow`, all drawing the kit scrollbar (`MD3ScrollBars`) | Every scrolled page and panel: the Prepare sidebar, every settings page, every Preferences page, the Device tab, the command palette, long message boxes and dialogs; the kit lists; every table: the Objects list, the project files list, the unsaved changes comparison, the Workspace lists, Config profiles, Export, Version history, the notification centre, the print host queue; and every multi-line text box: `TextArea` (update notes, logs, scripts, notes, prompts), the settings' G-code fields and the regex builder's sample and results; and every HTML view: the body of a message box with a table or a link, System Information, About, the setup wizard's HTML pages and the other HTML notes |

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
- **Group boxes.** `MD3GroupBox` is a `wxStaticBox` that paints its own border band: a 1 px
  OutlineVariant outline with small rounded corners, and the title in the kit's small title face in
  OnSurface (OnSurfaceVariant while disabled). The native box still does everything
  `wxStaticBoxSizer` relies on, so the controls inside lay out as before. The sidebar's extruder
  groups (`StaticGroup`) take OutlineVariant and OnSurfaceVariant instead of a legacy grey.
- **Bed shape.** The kit combo chooses the page of a `wxSimplebook`, where a `wxChoicebook` put a
  native choice over its pages. The dialog, its pages and its buttons lost their fixed white, and a
  missing texture or model file is named in the Error role instead of a raw red.
- **OK and Cancel.** The standard button sizers make native buttons. Kit buttons with the standard ids
  (a Filled OK, an Outlined Cancel) take their place, so the dialog's own OK, Cancel and Escape
  handling still applies.
- **Scrollbars.** Windows drew a 17 px grey strip with a thin grey thumb in every scrolled window, in
  both themes. `MD3ScrolledWindow`, used wherever a `wxScrolledWindow` was, the kit `ListBox`, and
  `MD3DataViewCtrl` and `MD3DataViewListCtrl`, used for every table, and `TextAreaEditor`, used for
  every multi-line text box, create their window without a
  Windows scrollbar and draw the kit scrollbar instead: a 10 px strip in
  the colour of the surface behind it, holding a fully rounded thumb inset 2 px, OutlineVariant at
  rest and Outline while the pointer is on it or drags it. A bar takes its strip beside the content,
  where the Windows bar sat, so nothing is laid out under it, and the content gains the 7 px the
  Windows bar took beyond that. Dragging the thumb moves the content with it; pressing the track pages
  towards the pointer and repeats while the button is held. The wheel, the keyboard and scrolling a
  focused field into view work as before, because wx's own scroll logic still does the scrolling and
  only tells the kit strip where to draw. With Windows high contrast on, the strip uses the system
  window and text colours. A table's own scroll logic scrolls its rows and its header as before, and the
  arrow keys still move its selection. A multi-line text box is a Windows edit control, which keeps
  following the caret, the wheel and the keyboard by itself; `TextAreaEditor` reads its first visible
  line, its line count and the lines that fit after every message that can move the text, draws the
  strip from them, and scrolls the edit control by lines when the strip is dragged or paged. An HTML
  view (`wxHtmlWindow`) is a scrolled window too, and `MD3HtmlWindow` gives it the same kit strip: the
  body of a message box that shows a table or a link, System Information, About, the setup wizard's
  HTML pages and the other HTML notes.

## What stays native, and why

- The system file dialogs, which bring the user's places, previews and the Windows shell with them.
- Native classes that no build can show: the SLA archive import's file picker (the import has no
  menu entry), the check-list combo popup in `wxExtensions` (no caller), and the monitor base panel
  with its splitter (never constructed).

## Verification

- `node --test ui-md3/tests/native-controls.test.mjs` refuses a native tab control, report list,
  check list, month calendar, tip window, info bar, group box, choice book, tree control or standard
  button sizer anywhere in the GUI, and checks each kit replacement.
- `node --test ui-md3/tests/scrollbars.test.mjs` refuses a new `wxScrolledWindow`, `wxHtmlWindow`,
  data view table, data view list or multi-line `wxTextCtrl`, checks that the kit scrollbar never hands a bar to Windows
  and reserves its strip in the non-client area, and that code which left room for the Windows bar
  leaves room for the kit one.
- The layout probe records, for every window, whether it shows a Windows scrollbar or a kit one
  (`scrollbars`: `native_v`, `native_h`, `kit_v`, `kit_h`), so a probe dump of a released build
  lists every Windows scrollbar still on screen.
- A released build has not been driven through these surfaces yet.
