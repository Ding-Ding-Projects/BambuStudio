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
- Surface ids are config keys: a lowercase letter first, then lowercase letters,
  digits, `_`, `.` or `-`, at most 64 characters. Any other id still collapses
  but is not stored.
- Report every filter that currently excludes items through
  `SetActiveFilters()`. Leave it empty for statistics.

## Tests

`tests/collapsible_filters/collapsible_filters_tests.cpp` checks the starting
states, persistence across a simulated restart, fallback for unknown stored
values, rejection of unsafe surface ids, and that a collapsed row always
discloses its active filters, including unnamed ones.

## Still to verify in the built application

Focus ring visibility, screen reader announcements and the layout after a
toggle need to be observed in the built Windows application.
