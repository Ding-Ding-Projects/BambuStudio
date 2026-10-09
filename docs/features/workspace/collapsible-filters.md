# Collapsible filters and statistics

Search bars, filter rows and statistics panels can be folded away so the list
or view they belong to gets the room. Each one sits under a small header with a
chevron; activate the header to hide or show the controls below it.

## Behaviour

- **Starting state.** Controls that change what a collection shows (a search
  bar, filter chips, date or category filters) start expanded. Panels that only
  describe the collection (counts, totals, statistics) start collapsed.
- **Remembered.** The choice is stored per surface and comes back after a
  restart. It is kept in the `collapsible_filters` section of
  `BambuStudio.conf`, one `expanded` or `collapsed` value per surface. An
  edited or unknown value falls back to the starting state above.
- **Nothing is hidden silently.** While a filter row is collapsed and any of its
  filters still narrows the list, a line under the header says so, for example
  `Active filters (2): Errors · Search: abc`. Up to three filters are named; the
  rest are counted (`+2 more`). The header itself also carries the count, for
  example `Search and filters (2 active)`, in both states. Clicking the line
  expands the row.
- **Keyboard.** The header is an ordinary focusable button in the tab order.
  <kbd>Space</kbd> or <kbd>Enter</kbd> toggles it, and it draws the shared
  Material focus ring while focused. Collapsing a row while focus is inside it
  moves focus to the header, so focus never stays on a hidden control.
- **Screen readers.** The header reports itself as a button that is either
  expanded or collapsed, its name includes the active-filter count, and its
  description repeats the disclosure line. Changing the state, the count or the
  line raises the matching accessibility change event so assistive technology
  announces it. The default action is named **Expand** or **Collapse**.

## Where it is used

Each surface keeps its own remembered state.

| Surface | Header | Counted as active while collapsed |
|---|---|---|
| Notification centre | Search and filters | search text, a level chip other than **All levels**, dismissed toasts hidden |
| What's new (changelog) | Search and filters | search text, a **From** or **To** date |
| Project version history | Search and filters | search text, a category or status other than **All**, a device identifier, a **From** or **To** date, the store shown in the Git graph view |
| Device farm toolbar | Search | search text (the header sits in the toolbar row) |
| Config profiles and backup | Search | search text |
| User presets | Search | search text |
| Print host upload queue | Search | search text (the queue selects matches rather than hiding rows) |
| Local identity history | Search and filters | search text, a **From** or **To** date, each action type switched off |
| Status Hub | Search | search text |
| Export dialog | Search | search text |
| Sidebar object search | Search | search text; the header hides and shows with the object list |
| Sidebar ink slot search | Search | search text; it folds away with the ink section as before |
| Sidebar settings search (compact card and full settings bar) | Search | nothing: these open a results list instead of hiding settings, so there is never a hidden filter to report |

In the sidebar, <kbd>Ctrl</kbd>+<kbd>F</kbd> still focuses the object search.
If the search is collapsed, the shortcut expands it for the current session
only; the remembered choice does not change.

## Panels on the 3D canvas

Two panels are drawn on the 3D canvas rather than as native controls: the
preview's legend and statistics dock and the assembly view's **Assembly
Structure** panel. They follow the same rules:

- **Remembered.** The fold state is stored as `preview_legend` and
  `assembly_structure` in the same `collapsible_filters` section and comes back
  after a restart. Both start expanded: the legend dock carries the view-mode
  chips and display options that change what the preview shows, and the
  structure panel is the working step tree. The existing
  `use_last_fold_state_gcodeview_option_panel` setting (on by default) still
  decides whether the preview restores the stored state; when it is off, each
  newly loaded G-code opens the dock expanded, as before.
- **Keyboard.** With the canvas focused, <kbd>Shift</kbd>+<kbd>L</kbd> folds or
  unfolds the legend dock in the preview and the structure panel in the assembly
  view. Plain <kbd>L</kbd> keeps its preview meaning (one layer mode of the
  vertical slider). After the shortcut, the panel's toggle shows a Primary focus
  ring; the next mouse press on the canvas hides it again. The shortcut is
  listed in the keyboard shortcuts dialog under **Preview**.
- **Screen readers.** The canvas window exposes each panel toggle as a push
  button named **Legend and statistics** or **Assembly Structure**, reported as
  expanded or collapsed, placed where it is drawn and activatable from
  assistive technology. After the keyboard shortcut the toggle receives
  accessibility focus and then a state change event, so the new state is read
  out; a mouse toggle raises the state change event.

## For developers

The shared widget is `CollapsibleFilterBar`
(`src/slic3r/GUI/Widgets/CollapsibleFilterBar.{hpp,cpp}`). Its rules live in the
toolkit-free `src/slic3r/GUI/Widgets/CollapsibleFilterState.hpp` so they can be
tested without wxWidgets.

```cpp
auto *filters = new CollapsibleFilterBar(this, "my_surface", _L("Search and filters"),
                                         CollapsibleFilterBar::Purpose::Narrows);
m_search = new SearchField(filters->GetBody(), _L("Search items"));
filters->GetBodySizer()->Add(m_search, 0, wxEXPAND);
root->Add(filters->GetSectionSizer(), 0, wxEXPAND | wxBOTTOM, gap);

// Whenever the filter changes:
filters->SetActiveFilters({_L("Errors"), wxString::Format(_L("Search: %s"), query)});
```

- Create the collapsible controls with `GetBody()` as their parent. Collapsing
  hides that panel only, so the shown or hidden state a host gives its own
  controls is never changed.
- Use `Purpose::Describes` for statistics and summaries so they start
  collapsed.
- The bar follows the host background when it repaints and rescales itself on
  a DPI change, so hosts need no extra theme or rescale calls (calling
  `SyncTheme()` or `Rescale()` from an existing pass is harmless).
- Surface ids are config keys: a lowercase letter first, then lowercase letters,
  digits, `_`, `.` or `-`, at most 64 characters. Any other id still collapses
  but is not stored.
- Report every filter that currently excludes items through
  `SetActiveFilters()`. Leave it empty for statistics. The static helpers
  `SearchFilterLabel()`, `FilterLabel()` and `ExcludedFilterLabel()` build the
  labels; values are shortened to 32 characters.
- `Layout::Inline` puts header, disclosure line and controls on one row for
  toolbars. `ShowSection()` hides or shows the whole section with the collection
  it belongs to, and `SetExpanded(true, false)` reveals the controls without
  storing the choice.

Canvas panels use `CollapsibleFilters::CanvasDisclosure` for their state and
`CanvasDisclosures::publish()` / `announce()`
(`src/slic3r/GUI/Widgets/CanvasDisclosures.{hpp,cpp}`) to keep the canvas
accessible object in step with what ImGui drew.

## Tests

`tests/collapsible_filters/collapsible_filters_tests.cpp` checks the starting
states, persistence across a simulated restart, fallback for unknown stored
values, rejection of unsafe surface ids, and that a collapsed row always
discloses its active filters, including unnamed ones.
`collapsible_filters_contract.test.mjs` checks that the widget wires the rules to
the configuration, the keyboard and the accessible object, and
`collapsible_filters_adoption.test.mjs` checks that every surface in the table
above creates its controls inside the collapsible body, lays the section out and
reports its active filters. `canvas_panels_contract.test.mjs` checks that the
two canvas panels store their state, answer <kbd>Shift</kbd>+<kbd>L</kbd>, draw
the focus ring and publish and announce their toggles; setting
`COLLAPSE_SOURCE_REF` runs it against another revision.

## Still to verify in the built application

Focus ring visibility, screen reader announcements, the layout after a toggle
on each surface above, the theme of the collapsible body after a light or dark
switch, and the state surviving a restart need to be observed in the built
Windows application. For the canvas panels that also covers
<kbd>Shift</kbd>+<kbd>L</kbd> in the preview and the assembly view, the focus
ring position at every display scale, and what a screen reader reads after the
shortcut and after a mouse toggle.
