# Tabbed settings and the shared tab strip

Every tabbed surface in the application renders through one shared,
browser-style Material Design 3 tab strip: `src/slic3r/GUI/Widgets/TabStrip.{hpp,cpp}`.
The project tab bar (`ProjectTabBar`) is now a thin index-based face over it,
and the Preferences dialog presents its sections as tabs on the same strip
instead of a bespoke navigation rail. The data rules live in a header-only,
GUI-free model, `Widgets/TabStripModel.hpp`, so they are unit-tested without a
running application (`tests/tab_strip`).

## Behavior

### Dock edge

- The strip docks to **Left, Right, Top or Bottom**. The edge is chosen from
  the strip's context menu (**Dock tab strip ▸**) and is persisted per surface.
- **Left is the default for settings surfaces** (a screen is wider than it is
  tall and a label is wider than it is high, so a vertical strip shows more
  tabs legibly). **Top is the default for the project tabs.**
- Docking is an orientation change, not a rotation: labels are never drawn
  sideways. A vertical strip is a 230 px rail (SurfaceContainerLow, 1 px
  OutlineVariant divider on the content-facing edge, 44 px NavItem pills with
  the SecondaryContainer selected treatment); a horizontal strip is a 52 px bar
  (Surface fill, Brand-primary active underline). Hosts re-place the strip in
  their sizer on `EVT_TABSTRIP_DOCK_CHANGED` (`MainFrame::place_project_tabbar`,
  `PreferencesDialog::place_settings_strip`).

### Overflow

- When the displayed tabs exceed the strip's main axis, the ones that do not
  fit move into an overflow **More tabs** Material menu instead of being
  clipped. The arithmetic (`MD3::Tabs::compute_overflow`) is shared by both
  orientations: the caller passes widths for a horizontal strip and heights for
  a vertical one. Pinned tabs are always kept visible; the overflow button's
  own space is reserved only when something overflows.
- On hide-on-close surfaces (settings) the same menu also lists the tabs the
  user hid from the strip, each marked *(hidden, click to restore)*.

### Reordering

- Drag a tab along the strip's axis to reorder it; **Ctrl+Shift+Arrow** (the
  arrows that match the orientation) moves the focused tab by keyboard.
- Moves are clamped to the tab's own region: an unpinned tab can never be
  dropped into the pinned region and vice versa.

### Pinning

- **Pin tab / Unpin tab** (context menu, **Ctrl+P**). Pinned tabs occupy a
  dedicated leading region, stay visible when other tabs overflow, carry a pin
  marker, lose their close affordance, and are excluded from single closes,
  from **Close tabs containing / not containing text** by default, and from
  Delete / Ctrl+W. The bulk-close dialog has an explicit **Include pinned tabs**
  choice whose effect is previewed before anything closes.

### Grouping

- **New group with this tab…** creates a named, coloured group; **Move into
  group…** opens a picker dialog (never an inline menu list) listing existing
  groups with name, colour and member count, an honest empty state, a
  **New group…** path and its own `SearchField` with the anchored regex
  builder. Arrow keys, Enter and Escape operate it; focus returns to the strip.
- Group headers render before each group run (a colour chip, the name and a
  chevron). Clicking a header collapses or expands the group; its context
  menu offers **Rename group…**, **Change group colour…** (the MD3 infinite
  colour picker), **Collapse / Expand group**, **Search tabs in this group…**,
  **Remove group (keep tabs)** and **Edit group appearance…**.
- A collapsed group displays only its active member. A search result inside a
  collapsed group is *revealed* (shown alongside the active member) without
  changing the collapsed preference; the reveal is transient and never saved.
- Moving a tab into a collapsed group leaves that group collapsed.

### Persistence

Order, pinned state, hidden state, group membership, group names / colours /
collapsed state, the dock edge and the active tab are stored as one JSON
document per surface in `BambuStudio.conf` under `[tab_strips]`
(`preferences`, `projects`). Surfaces whose tabs are built in code
(Preferences) call `TabStrip::LoadLayout()`, which applies the saved layout to
the tabs by stable id (`adopt_layout`: unknown saved ids are dropped, new
sections keep creation order after the known ones). The project strip stores
its tabs entirely in the layout (`LoadTabsFromLayout`) and imports the legacy
`[project_tabs]` / `[tab_groups]` sections once.

### Closing

- **Settings surfaces**: "close" means *hide from the strip*. The section stays
  in the model and in the search index, is restorable from the overflow menu,
  and reappears when a settings search teleports to one of its rows.
- **Project tabs**: close is real. The strip raises `EVT_TABSTRIP_CLOSE_REQUEST`
  (translated to `EVT_PROJECT_TAB_CLOSE`) and MainFrame runs its unsaved-work
  confirmation before removing the tab.

### Bulk close

Two actions live in every strip / tab / group context menu:
**Close tabs containing text…** and **Close tabs not containing text…**. Both
open the same dialog: a `SearchField` (plain text by default, the ".*" toggle
and the `tune` builder for regex), a **Close the tabs that do NOT contain the
text** switch so the inverse negates exactly the same predicate, the
**Include pinned tabs** choice, and a live preview that names the match mode
("Containing text · plain text" / "Not containing text · regex"), the exact
count and every affected title. The Close button stays disabled on an empty
query, an invalid pattern or zero matches. Matching is against the visible tab
label only.

### The four tab-discovery searches

Each is a dialog with its own `SearchField` and anchored regex builder, placed
beside the strip that opened it:

1. **Search tabs…** (**Ctrl+Shift+K** while the strip has focus, the search
   icon at the strip's trailing end, or the context menu) — the current strip.
2. **Search tabs in this group…** — one group only (from a tab in that group or
   the group header).
3. **Search tab groups…** — groups by their visible names.
4. **Search all tabs…** — the master search over every strip the application
   owns (`TabStrip::Registry()`: project tabs plus every open settings strip).

Results read `Surface › strip › group (collapsed) › title [pinned] [hidden]`;
Enter or **Go to tab** activates the hit, revealing a tab inside a collapsed
group without destroying the collapsed preference and un-hiding a hidden one.

### Context menus and shortcuts

Every item that has a working shortcut shows it, right-aligned, and the
shortcuts are the ones the strip's own key handler implements while the strip
has focus: **Ctrl+P** pin / unpin, **Ctrl+W** or **Delete** close / hide,
**Ctrl+Shift+K** search tabs, **Ctrl+Shift+E** edit tab appearance,
**Ctrl+Shift+Arrow** reorder, **Enter / Space** activate, **Home / End**,
**Shift+F10 / Menu** open the menu. The menus carry **Edit tab appearance…**,
**Edit group appearance…** and **Edit tab strip appearance…**; Shift+right-click
on a tab, a group header or the strip opens the matching editor directly.

### Appearance editor hook

The editors are wired through a weak hook so this lane does not depend on the
appearance-editor lane:

```cpp
TabStrip::SetAppearanceEditorHook([](wxWindow *anchor, const std::string &element_id) {
    AppearanceEditor::open_for(anchor, element_id);
});
```

`element_id` is `tabstrip:<surface>`, `tab:<surface>:<tab id>` or
`group:<surface>:<group id>`. Until the hook is installed the menu items are
present and are a no-op (`TabStrip::HasAppearanceEditorHook()` reports the
state so a host can label them accordingly).

### Accessibility

- The strip exposes a `PAGETABLIST` with one `PAGETAB` child per displayed tab
  (`TabStripAccessible`), names that include the pinned state and group, the
  selected / focused states, screen locations and a *Switch* default action.
- **Orientation follows the axis, not the markup**: a vertical strip announces
  itself as a vertical tab list and moves focus with **Up / Down**; a
  horizontal one uses **Left / Right** (`MD3::Tabs::arrow_step`). Focus is
  roving (the strip owns keyboard focus and paints a Primary focus ring on the
  focused tab), so Tab leaves the strip in one step.
- The strip runs no animation of its own, so reduced-motion preferences are
  honoured trivially; Material menus and dialogs it opens follow
  `MD3::Motion::reduced()`.
- Labels ellipsize at the end and carry the full title as tooltip; nothing is
  rotated, and the vertical rail collapses tabs into the overflow menu rather
  than clipping at short heights.

## Surfaces

| Surface | Strip | Default edge | Close means |
| --- | --- | --- | --- |
| Project tabs (`ProjectTabBar`, MainFrame) | `projects` | Top | real close, MainFrame confirms |
| Preferences sections (`PreferencesDialog`) | `preferences` | Left | hide from strip, restore from overflow |

The Preferences **Appearance** section is a page of the Preferences strip, so
it is covered by the same tabs. Preferences' existing settings search (row
filtering, highlight, teleport) is unchanged: a match on another page switches
the book *and* reveals that page's tab in the strip (`Activate(id, emit=false)`).

### Surfaces that are deliberately not tabbed

- **Config profiles dialog** (`ConfigProfilesDialog`): a single list of profiles
  with a search field and actions; it has no sections a user navigates between,
  so there is nothing to tab. It keeps its `SearchField` with the regex builder.
- **Preset editors** (`Tab.cpp`, the print / filament / printer parameter
  pages): their sections form a hierarchical page tree (`TabCtrl`) driven by
  the preset system, with pages appearing and disappearing per printer
  capability. Converting that tree to a flat strip is a separate lane; it is
  recorded here so the gap reads as a decision, not an oversight.

## Configuration

`[tab_strips]` in `BambuStudio.conf`, one JSON value per surface key. Deleting
a value resets that surface to its shipped order, unpinned, ungrouped, all
sections shown, default dock edge.

## Failure modes

- Malformed layout JSON is ignored (`Model::from_json` returns false and the
  model is untouched); the surface starts from its code order.
- A saved layout naming a section that no longer ships drops that entry; a
  saved group id that no tab references is kept (empty groups are only removed
  when the user removes them).
- The bulk-close dialog fails closed: an invalid regex or empty text disables
  the action and says why; the shared matcher's fail-open behaviour on an
  invalid pattern is therefore never reached from this dialog.
- If a hidden settings section is the only match of a settings search, the
  teleport un-hides it so the row can be shown.

## Security considerations

Layout JSON holds tab ids, titles and (for projects) file paths only. Regex
evaluation goes through the bounded, isolated `BoundedRegex` worker with the
same limits as every other search field.

## Verification

- `tests/tab_strip/tab_strip_tests_main.cpp` (Catch2, wx core only): pinned
  region invariants, region-clamped reorder, group create / assign /
  contiguity / collapse / reveal / remove, hidden tabs, JSON round trip
  (order, pins, groups, colours, collapsed, edge, active; transient reveal not
  persisted; stable serialisation), rejection of malformed JSON, `adopt_layout`
  onto code-built tabs, overflow for horizontal (widths) and vertical
  (heights) strips with pinned always visible, bulk-close refusal of empty and
  invalid patterns, predicate + inverse partition with pinned excluded and the
  include-pinned override, search result shape (surface / strip / group /
  pinned / hidden / collapsed, group filter, group search), and the arrow-key
  orientation rule. Build recipe: see `tests/tab_strip/CMakeLists.txt`.
- Every changed `.cpp` was syntax-checked with `cl /Zs` against the GUI
  project's include directories and defines.

## Suggested articles

- [Browser-like project tabs](project-tabs.md) — the project surface of this strip.
- [Preferences auto-history](preferences-history.md) — the settings the strip navigates.
- [Non-blocking notifications](non-blocking-notifications.md)
