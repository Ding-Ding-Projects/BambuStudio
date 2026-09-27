# Tabbed navigation

The site separates its content into eight tabs reached from a persistent strip below the header,
modelled on browser chrome rather than on a document outline. Implementation:
[`ui-md3/site/tabs.js`](../../../ui-md3/site/tabs.js).

## Tabs

| Id | Label | Default group | Panel |
|:---|:---|:---|:---|
| `overview` | Overview | Product | Hero, four counted facts, what the site is |
| `screens` | Screens | Product | Ten restyled screens, image-led cards |
| `materialyou` | Material You | Product | Live theme, density and accent seed controls |
| `download` | Download | Product | Latest release, its real assets, verification commands |
| `changelog` | Changelog | Project | Every published release |
| `regex` | Regex lab | Tools | The full regex builder at full width |
| `settings` | Settings | Tools | Every preference, with its own search |
| `build` | How it is built | Project | Four steps, including the layout gate |

A tab is addressable: `?tab=<id>` selects it on load and the active tab is mirrored into the URL
hash, so a link to `#changelog` opens the changelog.

## Behaviour

- **Overflow.** Tabs that do not fit move into an overflow menu; they are never clipped. The menu
  also carries the per-tab actions and the tab search.
- **Reordering.** Drag a tab onto another to move it, or press <kbd>Shift</kbd> +
  <kbd>←</kbd>/<kbd>→</kbd> with a tab focused. Order persists.
- **Pinning.** A pinned tab drops its label, keeps its icon, and sorts ahead of unpinned tabs.
  <kbd>Alt</kbd> + <kbd>P</kbd> toggles it; so does the tab's context menu.
- **Groups.** Every tab belongs to one of three groups, shown as a seam in the strip and a label in
  the tab search. A tab's group can be reassigned from its menu, and the assignment persists.
- **Search.** The magnifier opens a searchable tab list wired to the shared regex builder: plain
  text by default, regex as an explicit opt-in, matching English and Cantonese labels and the tab
  id. At widths where nothing else fits, the search moves inside the overflow menu.
- **Persistence.** Order, pinning, grouping and the active tab are stored under
  `bambuStudio.site.v1` in this browser only.

## Accessibility

The strip is a real `tablist` of `tab` buttons controlling `tabpanel` sections. The active tab is
the only one in the tab order (`tabindex="0"`, the rest `-1`); <kbd>←</kbd>/<kbd>→</kbd> move and
activate, <kbd>Home</kbd>/<kbd>End</kbd> jump to the ends, <kbd>Escape</kbd> closes any open menu,
and the context-menu key opens the tab's own menu. Focus is visible on every control, and group
reassignment is exposed as `menuitemradio` items with `aria-checked`.

## The layout algorithm

Layout is measured, never estimated. `fits()` asks whether every visible tab **and** the trailing
search and overflow buttons still share the first row; `runLayout()` then escalates through four
stages until they do:

1. every tab with its label, no overflow button;
2. overflow button shown;
3. labels dropped (icon-only tabs);
4. tabs pushed into the overflow menu from the end, never the active one;
5. finally the tab-search button is withdrawn into the menu.

Two details are load-bearing and both were bugs first:

- **The strip wraps in CSS.** `flex-wrap: wrap` on `.tabstrip` and `.tabstrip-tabs` means that even
  with scripting broken, tabs move to a second row instead of overlapping or being clipped.
- **The debounce is a timer, not `requestAnimationFrame`.** A page that is never painted — a
  background tab, a headless capture, the deploy gate — never runs rAF callbacks, so a strip
  debounced that way would stay frozen in whatever state it was in when the icon font was still
  loading. Icon ligature names are far wider than their glyphs, so that state is "everything
  overflowed".

The icon and body faces are additionally requested through `document.fonts.load(...)` and the strip
re-measures when they arrive; `fonts.ready` alone can settle before a face this page has not yet
painted starts loading.
