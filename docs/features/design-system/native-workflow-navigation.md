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

## Shared neutral palette, separate visual unit

The following default roles implement the Studio Atlas porcelain/slate ramp.
They affect native and other consumers of `MD3::resolve`, plus the named legacy
neutral aliases in `StateColor.hpp`. This is a shared default-color change, not
completion of the remaining per-surface composition and control work.

| Role | Light | Dark |
| --- | --- | --- |
| Surface | `#f7f9fc` | `#151c25` |
| Surface dim | `#d6e0ec` | `#111821` |
| Surface bright | `#fafcff` | `#3b4c60` |
| Container lowest | `#ffffff` | `#101720` |
| Container low | `#eef2f7` | `#1a2430` |
| Container | `#e7edf5` | `#202d3b` |
| Container high | `#dfe7f1` | `#293849` |
| Container highest | `#d7e1ed` | `#344557` |
| On surface | `#172434` | `#e8eff8` |
| On surface variant | `#46576a` | `#b9c8da` |
| Outline | `#6d7e94` | `#899caf` |
| Outline variant | `#c4d0df` | `#43566a` |

The light outline is slightly darker than the initial reference's `#718298`:
the original pair against the highest container measures 2.969:1, while the
implemented `#6d7e94` measures 3.137:1. Essential boundaries need at least 3:1.
The subtle outline-variant remains a decorative separator, not the sole
identification of an essential control. Primary and supporting text meet 4.5:1
against every default neutral surface in both themes. These are source-color
calculations, not measurements of rendered state layers or user-selected colors.

`StateColor` still accepts historical raw light RGB values for compatibility.
Every compatibility-map output is a fixed point under repeated dark conversion.
When returning to light mode, current semantic neutral roles take precedence
over historical light aliases that share the same dark tone. Otherwise the
reverse map could resurrect an older default based solely on numeric key order.
Distinct surface/bright keys also retain those roles during conversion.

The six Brand accent roles, Preview/Device accents, error/inverse roles, user
seed generator, density metrics and appearance registry are unchanged. Saved
element and preset overrides are not rewritten or migrated. Arbitrary RGB values
outside the existing/default compatibility set pass through unchanged. As before,
the RGB-only compatibility route cannot distinguish a custom color deliberately
equal to a known default from that default; this change does not claim otherwise.

Run `node --test tests/native_navigation/neutral_palette.test.mjs` for nine focused
source-contract and color-math cases. The test resolves real constants and map
entries from the production files, checks both themes, neutral hierarchy, contrast,
forward/reverse registration, historical compatibility and dark idempotence, and
rejects deliberate low-contrast and double-remap mutations before checking the
original palette again. A narrow fingerprint protects the unchanged accent
resolver/generator source. These checks do not execute wxWidgets or prove native
theme transitions, persisted user-override rendering or screenshots.

This palette is preserved in a separate visual commit from navigation commit
`624e2ed52e2fe7afcb91961fcaefe9bb201ba15e`. A reviewed revert of the palette commit
restores its three shared-color files, this article addition and its focused test
without removing the workflow navigation API. Reverting the entire navigation
commit is not a design-only reversal: it also removes ordering and overflow
behavior on which Print integration depends. The central design change ledger
records the exact independently reversible palette commit after integration.
