# Native menu builder interaction verification

The installed native driver provides independent `menu-builder-root` and
`menu-builder-nested` scopes. The existing `menus` scope remains unchanged.
Separate invocations preserve the encrypted envelope's 30-image limit: the root
scope requires 22 observations and the nested scope requires 27, including the
initial ready observation. Every native input has its own subsequent capture.

Both scopes open the empty Prepare canvas context menu and exercise a matching
literal query, visible Clear control, no-match query and Escape recovery. They
enable regex mode, open the actual Regex builder popup, require keyboard focus
in its empty pattern entry, and type `^fixture$`. The owning menu must expose
that exact query. The Test pattern control opens the sample section; typing
`fixture` must produce the actual one-match status and result text.

The driver selects the pattern with native Ctrl+A, types an invalid `[` pattern,
requires the rendered unbalanced-set status and empty results, then replaces it
with the valid pattern and requires recovery. Modifier input is not assumed to
work: the actual entry value is checked after each replacement. Builder Escape
must close only the builder and return focus to its menu. Clear restores the
original row inventory; menu Escape returns focus to the invoking surface.

The nested scope first filters the parent to Add Primitive, opens that submenu,
and checks the parent's nonempty query after every child action. After child
dismissal, two separate Escape inputs clear and dismiss the parent, with focus
checked again. No menu command executes and no model, printer transfer, clipboard
operation or external assistance route is used.

## Evidence boundaries

Inputs use the existing owned hidden-desktop native route. UI Automation reads
identify exact visible controls and inspect values; no UI Automation mutation,
application command injection or generated image substitutes for interaction.
Missing, ambiguous, offscreen or disabled controls stop the scope. The driver
does not scroll blindly to find a sample field. Native DPI checks, measured
overflow checks, isolated profile, process teardown and encrypted transport
remain mandatory. Raw labels, queries, geometry and images stay in the existing
restricted evidence bundle until privacy and genuine-pixel review.

Implementation is source-only preparation. No local application, compiler or
tests were executed. Each scope still needs the exact-source installed hosted
run, authenticated bundle opening and per-action pixel review. A source review
does not establish native provider names, actual modifier delivery, popup focus
restoration, sample visibility, intermediate animation or readability.

Run both scopes across English, Cantonese and bilingual modes, light/dark themes,
normal and measured minimum dimensions, and actual supported display scales.
Keep unavailable tuples explicit. Existing `menus` remains required for its
regex full-inventory and keyboard traversal observations. These builder scopes
do not claim canvas/ImGui menu or every application context-menu coverage.
