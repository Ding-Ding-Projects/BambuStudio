# Material context menus

Every context menu in the desktop app is rendered by the Material Design 3 menu widget
(`src/slic3r/GUI/Widgets/MD3Menu.{hpp,cpp}`) instead of the native Windows popup. The `wxMenu`
stays the data model: ids, labels, accelerators, check and radio state, `wxEVT_UPDATE_UI` enable
rules and submenus all keep working unchanged, and every existing `Bind(wxEVT_MENU, …)` handler still
fires.

## Behavior

- **Surface.** Surface-container fill, 1 px outline-variant frame, 12 dp corners, 24 dp leading slot
  for an icon, check mark or radio glyph, body-small labels, caption-sized shortcuts right-aligned,
  a chevron for submenu rows, an 8 % state layer on hover and a secondary-container fill for the
  keyboard selection. Disabled rows keep their label at reduced emphasis. Separators are 1 px lines
  with 8 dp padding and collapse when filtering would leave them leading, trailing or doubled.
- **Search field.** A menu with six or more actionable rows carries the shared `SearchField` pill at
  the top, with the regex builder behind its `.*` toggle exactly as every other search surface.
  Typing filters the rows locally; the actions behind the rows never change. A submenu row stays
  visible when any of its descendants match. Typing while the list has focus redirects into the
  search field.
- **Shortcuts.** Each row shows the shortcut registered on its `wxMenuItem` (parsed from the
  accelerator, falling back to the `\t` suffix of the label) so the menu documents the faster route
  to every command it lists. Screen readers receive it through `GetKeyboardShortcut`, not as extra
  label text.
- **Submenus.** Open to the right of the parent row, top aligned, flipping to the left at the
  display edge; hover opens after 200 ms (immediately under reduced motion), Right or Enter opens,
  Left or Escape closes one level. A parent refuses to dismiss while a child is open.
- **Bounds.** The surface never covers its anchor: placement tries below, above, right and left
  before clamping into the display, and a menu taller than the free height scrolls inside its card.
- **Keyboard and focus.** Up/Down/Home/End/PageUp/PageDown move over actionable rows, Enter or Space
  activate, Escape or an outside click dismiss, Tab cycles between search and list, and focus returns
  to whatever had it when the menu opened. Mnemonics (`&E`) activate only while the search field is
  empty.
- **Activation order.** The item's `wxEVT_MENU` handler runs synchronously before the blocking
  `MD3::PopupMenu` call returns, matching the native Windows order that the plater's right-click
  menu relies on. Checkable items flip their `wxMenuItem` state before the event is sent.
- **Language modes.** In bilingual mode a row shows `label · secondary` when it fits and moves the
  secondary text into the row tooltip otherwise. The search placeholder and empty state are
  localized.

## Configuration

There is no setting; the widget replaces the native popup everywhere. Constants in `MD3Menu.hpp`:
`kSearchThreshold` (6 rows), `kSubmenuHoverDelayMs` (200), `kDrawShadow` (false: frame-only
elevation until a layered-window shadow lands).

Entry points for code: `MD3::PopupMenu(owner, menu, screen_pos)` (blocking, sends events),
`MD3::PopupMenuSelection(owner, menu, screen_pos)` (blocking, returns the id, sends nothing),
`MD3::PopupMenuBelow(anchor, menu)` (anchored under a button). `Plater::PopupMenu` routes through the
first, so the object list, preset combo boxes, the 3D scene and the ink rows all share one surface.
The tray icon's `wxTaskBarIcon::CreatePopupMenu` is the one deliberate exception: the shell owns
that popup.

## Failure modes

- A handler that destroys the owner window while the menu is open: the popup binds the owner's
  `wxEVT_DESTROY` and closes itself; the nested event loop exits exactly once.
- An `UpdateUI` rule bound on a window outside the owner's handler chain: the row keeps the
  `wxMenuItem`'s stored enabled state. Pass the same owner the native call used (the main frame for
  plater menus).
- A label rewritten by the vocabulary layer that has no catalog entry: the row falls back to the
  primary label alone.

## Security considerations

The widget evaluates the search pattern through the shared bounded regex engine; no menu text or
query leaves the process.

## Verification

- `tests/md3_menu` (Catch2): label/shortcut split, snapshot kinds and `UpdateUI` enable state,
  filter separator collapsing and submenu retention, `place_root` never intersecting the anchor or
  leaving the display at all four corners, `place_submenu` flipping at the right edge. Last run:
  92 assertions in 7 test cases, all passed.
- Hidden-desktop captures from the built payload:
  `docs/screenshots/md3-everything/topbar-file-menu--en-light-comfortable--after.png` (File menu with
  search pill and shortcut column), `topbar-file-menu-filtered--…` (typing `exp` leaves the Export
  row), `ink-row-menu--…` (ink ⋯ menu with Delete, Decompose Color and Merge with disabled for a
  single ink).
- Not verified by drive: keyboard navigation and Escape on the hidden desktop. The cheap headless
  key route does not reach the popup (the same helper limitation recorded for `Ctrl+Shift+F`); the
  key handling is covered by code review and the unit tests only.

## Suggested articles

- [Regex builder](regex-builder.md) — the builder behind the menu search pill.
- [Appearance customization](appearance-customization.md) — where the surface tokens come from.
- [Command palette](command-palette.md) — the other searchable command surface.
