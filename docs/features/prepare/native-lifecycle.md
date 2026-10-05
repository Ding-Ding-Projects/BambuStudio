# Native menu and printer selection lifecycle

The Prepare printer card opens its preset dropdown through an explicit visible
focus owner. A selection snapshots its row before closing the child and root
popup surfaces. Close notifications run before selection callbacks. A weak
reference prevents delivery after owner destruction, and an item revision rejects
selections when closeup rebuilt the list, including identical-looking rows.
Layout, bitmap and flag refreshes do not change this structural item revision.

Blocking Material menus finish their nested event loop and schedule popup
destruction before delivering the chosen command. Submenu commands retain the
menu that owns their bindings. Delivery uses a surviving top-level invoking
window. The automatically inserted appearance command preserves its element and
weak anchor separately before removing the temporary menu item.
Close delivery state is captured before focus restoration or platform dismissal,
both of which may synchronously destroy a popup. Weak references protect the
remaining dismissal and delivery steps. Reused menus restore their previous
invoking window after command delivery rather than retaining a temporary owner.

Printer selection preserves the current printer when no matching model variant
exists. Canceling the unsaved-preset dialog stops configuration application,
plate redistribution, saved mapping changes and automatic flush calculations.

Physical filament rows map to configuration slot indices. Their menu callbacks
read the current mapping rather than the row's original index. A delete resolves
its target once, confirms that target, and aborts if the slot list changed during
confirmation. Mixed-slot identity comes from project flags rather than the count
of visible physical rows. The last physical filament remains protected. During
row destruction the parent owns sibling teardown instead of the combo scheduling
those same siblings for destruction again.
Deferred row-title layout work is queued on the sidebar itself, so destroying
the sidebar also removes its pending callbacks. Row rebuilding uses physical
slot mappings for every deletion, including interleaved mixed slots.

## Verification and limits

`dropdown_lifecycle_tests` compiles the actual production dispatch and combo
adapter bodies against deterministic event and lifetime doubles. It covers
root/submenu order, stale item generations, valid selection after measurement-only
invalidation, disabled rows, invalid rows, owner
destruction during closeup, child close destroying root, and selection destroying
its popup. These tests validate callback order and ownership decisions. They
also compile production menu focus/finalization bodies to exercise destruction
during focus restoration, platform dismissal and the close callback, along with
single close delivery and suppressed popup-stack dispatch. The deterministic
tests do not substitute for native wxWidgets backend interaction.

`node --test ui-md3/tests/native-lifecycle.test.mjs` supplies supplementary source
contracts and deliberate negative mutations. The full native build and real
interaction evidence must separately cover P1S/H2C switching, repeated selection,
clean and modified presets, save/discard/cancel outcomes, and physical/mixed
filament deletion. No runtime root cause or successful capture is implied by the
source changes or the deterministic tests.
