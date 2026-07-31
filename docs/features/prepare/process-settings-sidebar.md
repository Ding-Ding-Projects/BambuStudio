# Process settings in the Prepare sidebar

The Prepare sidebar shows process settings in two modes:

- **Simple** (default) — the compact MD3 Process card: a settings search pill, the process-preset
  SelectField, a `Quality / Strength / Support / Others` segmented control, a handful of curated
  rows, and an **Advanced settings** button.
- **Advanced** — the *full* process-settings tree (`ParamsPanel`, reparented into the sidebar),
  carrying the `Global` / `Objects` mode switch, the category tab strip, and every option row. A
  **Simple settings** bar flips back.

The choice persists in the app-config key `sidebar_process_advanced`.

## Why the sidebar widens in Advanced mode

The full tree is the *settings-tab* layout. Each option row is a label plus a value field, and
neither half reflows: the row keeps its natural width or it gets cut. Measured live at the density
default, the `Layer height` row lays out **1234 px wide inside a 348 px sidebar**.

So flipping to Advanced asks the dock for `ADVANCED_SIDEBAR_WIDTH` (480 dip) via
`Plater::request_sidebar_width()`. That request is deliberately weak:

| Behaviour | Reason |
| --- | --- |
| Clamped to **55% of the frame** | A narrow window keeps a usable 3D canvas. |
| Never below the density default | Advanced mode must not make the sidebar *narrower*. |
| `grow_only` on the way in | A sidebar you already dragged wider is left alone. |
| Shrinks back only on the explicit flip to **Simple** | That flip is a request for the compact width. |
| Sash stays draggable; the dragged width is persisted by the existing idle handler | The forced width is a starting point, not a lock. |

Startup re-asserts the width on the first **laid-out** size event, not in the constructor: the
`Plater::priv` constructor runs before the frame has a width, and a request made then clamps to the
compact default. `request_sidebar_width()` returns `false` while the frame is still too small to
size against, so the caller retries instead of latching a bad value.

## Horizontal scrolling is the backstop

Width alone is not a guarantee — a very narrow window, a large display scale, or the longest
localized strings can still overflow. The sidebar body therefore grows its **virtual width** when
the content genuinely cannot compress, and has a real horizontal scrollbar to grow into
(`update_sidebar_scroll_body()`).

Anything that *can* reflow still receives the client width, so the compact cards never acquire a
scrollbar they do not need. `update_sidebar_scroll_body()` also carries a re-entrancy guard:
`SetVirtualSize()` can add or remove a scrollbar, which resizes the client area and re-enters the
helper through the sidebar's own `EVT_SIZE`.

> [!IMPORTANT]
> Before this, the body was created `wxSHOW_SB_NEVER` for the horizontal bar with an x-scroll rate
> of `0` and `EnableScrolling(false, true)`. Clipped option values were not merely off-screen —
> **no amount of scrolling could reach them.**

## The starved-header failure this fixes

At the compact width the header row was over-subscribed. When a `wxBoxSizer` row cannot pay every
minimum it pays the fixed items in full and hands **zero** to whatever straddles the boundary, so
the `Process` title and the **Compare presets** button were not clipped — they were *absent*,
leaving `Advanced` wedged against the `Global` / `Objects` switch.

This is the same failure mode that removed the Print button (see `HANDOFF.md` §3.3). A starved
control leaves no visual gap, so "nothing overhangs" is not evidence that nothing is missing.

## Search

Both modes carry a settings search pill wired to the same `OptionsSearcher` — the same dialog, the
same `.*` regex toggle and tune builder popover as every other search surface. The compact card's
field is hidden along with the card, so Advanced mode has its own on the **Simple settings** bar;
otherwise the surface with the most options in it would be the only settings surface with no search.

Scope is `Preset::TYPE_INVALID`, so a query spans every indexed preset type and activating a result
jumps to the owning option in this tree.

## Object manipulation

The Object-manipulation card starts **hidden** and appears when a selection makes its values real
(`GizmoObjectManipulation::Cache::is_valid()`), then hides again when the selection clears. With
nothing selected every cell reads as an en dash, so the card was charging a screenful of sidebar
height to say nothing. Its divider is hidden and shown with it, so no orphan rule is left behind.

## Verification

Captured headlessly from the real Release build at 846 px — the width this host's display forces,
and the narrowest supported case:

| Before | After |
| --- | --- |
| ![Clipped sidebar](../../screenshots/sidebar-process/before-sidebar-clipped.png) | ![Readable sidebar](../../screenshots/sidebar-process/after-sidebar-readable.png) |
| Value fields sliced off the right edge, tab strip cut mid-`Support`, preset name truncated. | Values with their units, full tab strip plus overflow, full preset name. |

| Before | After |
| --- | --- |
| ![Starved header](../../screenshots/sidebar-process/before-header-starved.png) | ![Intact header](../../screenshots/sidebar-process/after-header-intact.png) |
| No `Process` title, no Compare button. | Title and `Global` / `Objects` switch laid out. |

Measured with `press.py controls`: the 3D canvas starts at **x=348** before and **x=461** after, and
`Object manipulation` is absent from the control list until something is selected.
