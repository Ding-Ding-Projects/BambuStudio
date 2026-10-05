# Context menus

Every context menu of the Windows app is the Material menu (`MD3::PopupMenu` in
`src/slic3r/GUI/Widgets/MD3Menu.cpp`): the same surface as the caption bar's menus, following the
theme, the density and the three language modes (English, Cantonese, and the bilingual
"English · 廣東話" pairs).

## Where they open

| Surface | Menu | Route |
| --- | --- | --- |
| 3D canvas, object list, plates, preset lists, assembly steps | object, plate and preset actions | `Plater::PopupMenu`, which wraps `MD3::PopupMenu` |
| Tab strip, caption bar, auxiliary list, device status, web tools | tab, menu bar and panel actions | `MD3::PopupMenu` or `MD3::PopupMenuSelection` directly |
| Every text field | Undo, Cut, Copy, Paste, Delete, Select all | the application-wide text menu (below) |
| Copyable device labels (printer name, serial number, version) | Copy | `enable_static_text_copy_menu` |
| Any element adopted by the appearance editor | Edit appearance... | added to each of the above by `MD3::PopupMenu` |

## Text fields

`MD3::EnableTextContextMenus(true)`, called by `GUI_App` before any window exists, installs an event
filter that answers every context menu request of a text entry: `wxTextCtrl`, the inner field of the
kit's `TextInput`, `SpinInput`, `TempInput` and `SearchField`, editable combo boxes, and search and
rich text controls. The filter handles the request itself, so the system's edit menu never opens, and
shows the Material menu with Undo, Cut, Copy, Paste, Delete and Select all, each enabled as the field
allows:

- a read-only field offers Copy and Select all;
- a masked field (a password, or the Smart home token, which is masked after creation) never offers
  Cut or Copy, so a secret cannot reach the clipboard through the menu;
- a request from the keyboard (the Menu key or Shift+F10) opens the menu under the field, and a
  right-click opens it at the pointer.

A read-only combo box is a list rather than a field and keeps its own behaviour. Shift+right-click on
an element adopted by the appearance editor still opens its editor directly. The filter is removed
during shutdown, while the event loop still exists.

The kit fields used to swallow the right-click to hide the system menu, which left them without any
menu from the mouse while the keyboard still opened the system one. They now let the right-click
through to the Material menu.

## Web pages

`WebView::CreateWebView` switches the browser's own menu (Back, Refresh, Save as, Inspect) off unless
the developer tools setting is on, and the Flushing volumes dialog's page, which is created directly,
switches it off as well. Internal builds may switch it on for debugging (`#if !BBL_RELEASE_TO_PUBLIC`).

## Verification

- `node --test ui-md3/tests/context-menus.test.mjs` checks that no `wxWindow::PopupMenu` is called
  outside the Material menu, the text menu's filter, items, masking and keyboard anchor, its
  installation at startup and removal at shutdown, the kit fields' right-click, the web pages' menu
  setting, and the What's new year field (the kit `SpinInput` in place of a native spin control,
  which drew a system box and opened the system edit menu).
- `scripts/md3/check-context-menus.py` runs against a released package on a hidden desktop. It opens
  Smart home and Preferences, asks their text fields for a menu with a right-click and with the
  keyboard request, captures what opens and records its window class. A menu drawn by Windows has the
  class `#32768`; the run fails on one, and on a request that opens nothing. On `md3-v162`, before this
  change, the Smart home URL field and the Preferences search field opened nothing on a right-click
  and the system's English edit menu on the keyboard request
  (`docs/screenshots/md3-everything/context-menu-smart-home-keyboard--en-light-comfortable--md3-v162.png`).
- Verified on `md3-v169` (target `087fe6f70`): in English, Cantonese and bilingual mode, 8 of 8
  requests per mode opened the Material menu, with its search field and its items in the mode's
  language, and none opened the system menu or nothing
  (`docs/screenshots/md3-everything/context-menu-smart-home-right-click--bilingual_en_yue_HK-light-comfortable--md3-v169.png`).

Window title bars keep the system menu of Windows (Alt+Space); it belongs to the window frame rather
than to the app's content.


## Search and recovery update

Every native Material menu and shared combo popup now exposes its own search field
and guided regular-expression builder, including one-item menus and nested groups.
Filtering preserves command IDs and original combo item indices. An empty result
shows "No matches."; Escape clears a nonempty query before closing the popup.
Tab reaches the query, regex toggle, builder and clear control. A popup remains
open while its builder is visible. Oversized builder contents scroll within the
display work area.

The shared motion curves use the Material standard cubic and emphasized two-part
path. Elapsed time and reduced-motion handling remain owned by the shared animator.

Verification status: source changes only. Hosted Windows builds and interaction
coverage are required for normal/minimum sizes, English/Cantonese/bilingual modes,
light/dark themes, and 100/125/150/200 percent scaling. This update does not claim
that matrix has run.
