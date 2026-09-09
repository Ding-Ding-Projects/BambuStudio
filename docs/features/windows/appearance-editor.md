# Per-element appearance editor

Every rendered element that has opted in can be restyled on its own: right-click
it and choose **Edit appearance...**, press **Ctrl+Shift+E** while it has focus,
or Shift+right-click it to skip the menu. A non-modal Material card opens
beside the exact element, tracks it while it moves, and writes every change to
a per-element registry that the live UI re-reads at once.

Source: `src/slic3r/GUI/Appearance/ElementStyle.{hpp,cpp}` (registry,
persistence, presets, wxWindow adopter) and
`src/slic3r/GUI/Appearance/AppearanceEditorPopover.{hpp,cpp}` (the card, the
context-menu helper, the shortcut).

## Behavior

### Opening routes

| Route | Where | Result |
| --- | --- | --- |
| **Edit appearance...** in a context menu | every Material menu whose owner was adopted (automatic), plus the object-list, plate, multi-selection and filament-row menus (explicit) | card opens anchored to the element the menu was opened on |
| **Edit tab appearance...** | project tab right-click menu (beside *Close tab*) | card opens for that tab |
| Shift+right-click | any adopted element, project tabs | card opens directly, no menu |
| **Ctrl+Shift+E** | focused control anywhere in the main frame, and inside dialogs whose controls are adopted | card opens for the focused control; an un-adopted control is adopted on the spot under `focused/<name>` |
| Command palette (Ctrl+F) | "Edit appearance of the focused element", "Apply appearance preset: ..." | same as the shortcut / preset switch |

The menu item shows its shortcut (`Ctrl+Shift+E`) right-aligned like every
other Material menu row, and the shortcut is listed under *Global shortcuts* in
the keyboard shortcuts dialog.

### The card

A `wxFrame` tool window (no taskbar entry, floats on its parent, no native
frame) painted as a SurfaceContainer card with a 1 px OutlineVariant border and
the rail radius. It is **non-modal**: the application stays usable while it is
open, and the element keeps re-rendering as values change.

- **Anchored.** Placement reuses `MD3::Menu::place_root` (below the anchor,
  else above, right, left, then shrunk) so viewport-edge collisions are
  handled exactly as menus handle them; the card never overlaps or detaches
  from its anchor. A 120 ms tick re-places the card when the anchor's screen
  rectangle changes, hides it while the anchor is scrolled off screen, and
  closes it when the anchor is destroyed.
- **One card.** Opening for another element re-targets the open card instead
  of stacking a second one.
- **Focus.** Escape or the close button closes the card and returns focus to
  the anchor. Ctrl+PageUp / Ctrl+PageDown move between sections; Tab walks
  every control.
- **Sections** (tab buttons at the top of the card):
  - *Typography* — searchable font list (installed faces from
    `wxFontEnumerator` plus the bundled Roboto, Roboto Mono, HarmonyOS Sans
    SC, NanumGothic, Source Han Sans JP and Symbola, tagged *(bundled)*) with a
    live preview line in the chosen face; size (pt, stepper + free entry);
    weight 100–900; italic, underline, strikethrough; letter spacing; line
    height.
  - *Colours* — text, background, highlight, border. Each swatch opens the
    Material colour picker (`MD3ColorPickerDialog`, with its colour
    translator); an unset colour shows *Theme default* and keeps the token.
  - *Shape & spacing* — border width, corner radius, padding, margin (px).
  - *Presets* — searchable preset list (shipped presets tagged, the active one
    marked), **Apply**, **Save as preset...**, **Delete** (user presets only),
    **Export theme...** / **Import theme...** (JSON via `wxFileDialog`).
- **Reset** at three scopes: a per-property undo button beside every control
  (enabled only while that property carries a user override), **Reset
  element** in the footer, and **Reset all** (confirmed; drops every override
  and returns to the *Material default* preset, keeping saved presets).
- The card **obeys its own customization**: its title, caption, section tabs
  and row labels are adopted elements (`appearance-editor.title`,
  `appearance-editor.caption`, `appearance-editor.tab`,
  `appearance-editor.label`), so the editor can restyle itself.
- Every search field in the card is the shared `SearchField` with the `.*`
  regex toggle and the anchored regex builder; plain text stays the default.

### Resolution model

A value for `(element id, property)` resolves through three layers, most
specific first:

1. the user's override for the element (`elements` in the file);
2. the active preset — its entry for the element, then its `"*"` entry;
3. the caller's base value (the MD3 token the widget would have used).

Ids may carry a parent after a slash: `project-tab/model` falls back to
`project-tab` at every layer, so one rule styles every tab while a single tab
can still differ. The `"*"` entry of a preset reaches every element.

Shipped presets are derived from the MD3 tokens only: *Material default*
(nothing overridden), *Large text* (15.5 pt, line height 1.4), *Compact text*
(12.5 pt, padding 6), *Rounded* (radius 20 = `Metrics::radius_home`), *Bold
labels* (weight 600). A shipped name can never be overwritten or deleted, and a
file that carries a shipped name is ignored for that name.

### Wired widgets (first cut)

| Element id | Widget | What the style reaches |
| --- | --- | --- |
| `menu.item` | every Material menu row (`MD3Menu.cpp`) | font family / size / weight / style / decorations, text colour |
| `project-tab`, `project-tab/<file stem>` | project tab strip (`ProjectTabBar.cpp`) | font, text colour, background (tab fill + hover) |
| `preferences.row/<AppConfig key>` (parent `preferences.row`) | every switch row label in Preferences | font, text colour, background |
| `topbar` | the top bar (`BBLTopbar`) | font, text colour, background of the bar window; context menu + shortcut |
| `object-list.row`, `object-list.plate`, `sidebar.filament-row` | object list / plate / filament menus | menu entry only — see gaps |
| `appearance-editor.*` | the editor's own chrome | font, text colour, background |
| `focused/<name>` | anything reached through Ctrl+Shift+E | font, text colour, background of that control |

Any widget can join with one call:
`ElementStyle::apply(window, "id", display_name)` remembers the window's
current font/colours as the base, applies the resolved style, re-applies on
every registry change, releases itself on `wxEVT_DESTROY`, and wires the
context menu, Shift+right-click and Ctrl+Shift+E. Owner-drawn widgets that
paint from tokens consult `ElementStyle::font_for(id, base)`,
`colour_for(id, role, base)` and `number_for(id, key, base)` at their paint
site instead.

### Known gaps

- Only the widgets in the table above are wired. Buttons, the sidebar labels,
  the notebook tab bar and the 3D-canvas ImGui chrome still paint from tokens
  alone; they can be adopted with `ElementStyle::apply` or the paint-site
  hooks, but no style reaches them yet.
- The object-list, plate and filament-row menus carry the entry, but the
  `wxDataViewCtrl` rows behind them are native and do not yet read the
  registry: editing those ids records the values (and the editor previews
  them on its own line) without changing the list's rendering.
- `letterSpacing`, `lineHeight`, `highlight`, `borderColor`, `borderWidth`,
  `radius`, `padding` and `margin` are stored and resolved but only a widget
  that measures or frames its own text can honour them; native `wxStaticText`
  ignores them. The editor says so on each section.
- The typography spinners are native `wxSpinCtrlDouble`s styled with the
  surface colours, not the Material `SpinInput` (which is integer-only).
- Anchor tracking is a timer poll (120 ms), not a move-event subscription, so
  a fast drag of the parent window shows the card catching up.

## Configuration

Storage: `data_dir()/appearance/element-styles.json`, loaded by
`AppearanceEditor::init` in `GUI_App::on_init_inner` (after the persisted
density/accent are applied) and written on every change through the editor
(coalesced per event-loop turn) or a palette preset switch. Writes go to a
`.tmp` beside the file and are renamed over it, so a crash mid-write leaves
the previous file intact.

Schema (`"schema": 1`):

```json
{
  "schema": 1,
  "activePreset": "Material default",
  "presets": { "<user preset>": { "*": { "fontSize": 15.5 }, "<element id>": { "foreground": "#146c2e" } } },
  "elements": { "<element id>": { "fontFamily": "Roboto Mono", "underline": true } }
}
```

Known properties: `fontFamily`, `fontSize` (pt), `fontWeight` (100–900),
`fontStyle` (`normal` | `italic`), `underline`, `strikethrough`,
`letterSpacing` (px), `lineHeight` (multiplier), `foreground`, `background`,
`highlight`, `borderColor` (`#rrggbb` or `#rrggbbaa`), `borderWidth`, `radius`,
`padding`, `margin` (px). This is the same shape as the site's `elementStyles`
model in `ui-md3/site/settings.js`.

Export writes the same document; import merges the file's user presets, takes
its element overrides and active preset, and keeps its unknown top-level keys.

## Failure modes

- **Missing file** — fresh registry; nothing is styled.
- **Unparseable JSON / not an object / no integer `schema` / schema newer than
  1** — the load is refused with a reason (`StyleLoadReport::error`), the
  registry keeps its current state, and a warning is logged. The file is not
  rewritten until the user changes something.
- **Unknown top-level keys and unknown element properties** — kept verbatim
  through load, edit and save, reported in `StyleLoadReport`, and listed in a
  message after an import. Nothing is silently dropped.
- **Unknown active preset** — falls back to *Material default* and is
  reported.
- **Unknown font family** — validated against the session font table before
  `SetFaceName`; an unknown face leaves the base font alone (wx would
  otherwise invalidate the font).
- **Out-of-range values** — sizes outside 4–96 pt are ignored, weights clamp
  to 100–900, an invalid colour string keeps the base colour.
- **Write failure** (read-only directory, full disk) — logged; the in-memory
  registry stays applied, so the session keeps its look.
- **Anchor destroyed while the card is open** — the card closes on the next
  tick without touching focus.

## Security considerations

- The file is user data in the profile directory; it is read with the same
  trust as AppConfig. Values only ever reach `wxFont`, `wxColour` and integer
  metrics — no paths, no commands, no URLs.
- Font faces are validated by `wxFontEnumerator::IsValidFacename` before use,
  in line with the GDI+ private-font rule in
  [Appearance customization](appearance-customization.md).
- Import reads a user-chosen JSON file only; it never fetches anything and
  never executes content.
- Regex search in the font and preset lists runs through the bounded regex
  worker (see [Regex builder](regex-builder.md)).

## Verification

- `tests/appearance/appearance_tests_main.cpp` (Catch2, target
  `appearance_tests`) covers: the property catalogue; nothing-resolves-by-
  default; set / resolve / per-property / per-element / global reset; parent
  inheritance through `/`; preset precedence (user override over preset entry
  over `"*"`); save-as-preset snapshotting; shipped presets being protected;
  JSON round trip preserving and reporting unknown keys (fixed point on
  re-parse); refusal of garbage, arrays, missing and newer schema without
  touching state; unknown active preset fallback; export → import equality and
  save → load equality; font / colour helper clamping; listener tokens.
  Last run: 105 assertions in 14 test cases, all passed (hand-linked against
  wx core/base with the same recipe as `tests/md3_menu`).
- Every changed translation unit passed `cl /Zs` against the project's
  include set (`MainFrame.cpp`, `GUI_App.cpp`, `GUI_Factories.cpp` and
  `Preferences.cpp` with the forced PCH include).
- Not yet verified: a driven run of the built application (right-click →
  card → live restyle) and captures of the card; the surface needs the next
  full build.

## Suggested articles

- [Appearance customization](appearance-customization.md) — the global theme,
  density, accent and font controls this editor layers on top of.
- [Material context menus](material-context-menus.md) — the menus that carry
  the *Edit appearance...* entry.
- [Material color picker & color translator](md3-color-picker.md) — the picker
  behind every swatch.
- [Regex builder](regex-builder.md) — the builder behind the font and preset
  search fields.
- [Command palette](command-palette.md) — the other route to the editor and to
  preset switching.
