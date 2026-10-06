# Native workflow navigation

The native notebook projects Prepare, Preview, Print, and Monitor into a leading
workflow group. `Notebook::SetWorkflowPages(prepare, preview, print, monitor)`
changes only the visual order. It never reorders the underlying book pages or
changes the numeric page IDs used by existing callbacks and command-palette
destinations. Every other page and trailing action remains available.

## Appearance and layout

The Studio Atlas treatment retains the 52-DIP navigation bar and uses the shared
semantic surface roles, a low tonal workflow group with a 1-DIP outline, rounded
16-DIP destination shapes, 20-DIP glyphs, and 14/13-pixel density-aware label styles. Selected
destinations use the paired PrimaryContainer and OnPrimaryContainer roles with
semibold labels. Hover and pressed states use the high and highest neutral
surfaces with their neutral foreground. The existing Button supplies the inset
keyboard focus ring. Comfortable and compact destination heights follow the
shared 40/32-DIP density metrics. The source preserves the current accent schemes;
the wider palette refresh is a separate change.

Labels reserve their complete measured semibold width even at rest, including
the existing local display-wording transformation. There is no fixed label-width
cap and shrinking is disabled. Destinations that do not fit move to the existing
Material menu surface, with its search, regex builder, keyboard navigation and
focus restoration. The active destination is prioritized for visible space. When
its entire label cannot fit, the overflow button carries its full accessible
name, selected treatment and marker, and the menu checks its row.

The layout uses an 8-DIP outer inset and 4-DIP destination gaps. Workflow group
decoration extends 1 DIP beyond its destination rectangles. The normal-density
40-DIP controls remain vertically centered with 6-DIP outer space. Compact
controls remain centered within the same shell height. Side tools retain their
measured trailing allocation. The parent frame's supported minimum viewport is
still authoritative; a width smaller than the fixed side tools plus overflow
target is not a new supported window size.

## Identity and interaction

Insertions shift stored workflow indices forward, and removals shift them back.
Removing a workflow page clears that role. A caller replacing a page must call
`SetWorkflowPages` again after the replacement, so an unrelated later page cannot
silently inherit the removed role. Prepare and Preview may share one content
window while retaining their distinct book indices.

The overflow menu binds weak button identities. It resolves the current page
index through the same button callback at activation, rather than retaining a
stale numeric position while the nested menu loop is open. Removed controls do
not activate another destination. A navigation resize hides only overflowing
controls, preserves focus on controls that remain visible, and transfers focus
to overflow when necessary. Visible keyboard order follows the visual order.

Project-tab pinning, reordering, groups, search, closing, persistence and related
actions remain owned by `ProjectTabBar` and the shared `TabStrip`; this notebook
change does not replace or remove them.

Selection and navigation dispatch remain immediate. The 250-ms selection wash
and marker are decorative, owner-bound and interruptible; the shared motion
policy settles them immediately under reduced motion. There is no canvas fade,
delayed command dispatch or continuous decorative animation.

## Verification and remaining evidence

`tests/native_navigation/navigation_layout_tests.cpp` compiles against the
production, wx-free `NotebookNavigation.hpp`. It checks workflow projection,
invalid and duplicate role inputs, insertion/removal, exact-fit thresholds,
selected auxiliary destinations, oversized labels, preservation of every
destination, unchanged measured widths and non-overlapping bounds across
100%, 125%, 150% and 200% scales and 343 viewport widths. Compile with the
repository's supported C++17 compiler, including `tests` and `src`; the test
defines its own Catch entrypoint. The integration owner registers it in the
repository test build separately.

This source unit has not been built as the native application or exercised in a
running window. It supplies no screenshot, accessibility-tree, menu interaction,
screen-reader, palette, theme or clipping evidence. The required built-artifact
matrix remains pending: normal and minimum viewports, English/Cantonese/bilingual,
both themes, both densities and every supported display scale. Source checks do
not establish visual parity. The complete application redesign also remains
broader than this navigation unit.
