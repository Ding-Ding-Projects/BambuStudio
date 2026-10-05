# Prepare settings scrolling

The Prepare sidebar body owns vertical and horizontal scrolling for its embedded settings page. The page delegates focus and search-result reveal to that body and disables its own scroll ranges. Standalone preset editors keep a bounded page viewport and their own scrollbars.

Reveal treats each axis independently. A group wider than the viewport no longer suppresses vertical reveal; an oversized target exposes its leading edge. Sidebar relayout preserves the current pixel origin, clamps it through the scroll helper when content shrinks, and re-reads the client area after scrollbar strip reservation. A per-window scope prevents synchronous size-event recursion without suppressing another surface's layout.

The sidebar still permits horizontal access for settings rows whose minimum width exceeds the viewport. This change does not redesign those option rows.

Verification: the existing scrollbar tests pass 10 checks; the sidebar-width tests pass 4 checks; prepare-scroll-owner.test.mjs passes 6 checks. Removing reveal delegation deliberately fails the ownership check, and restoring it passes. The arithmetic test executes the production scalar expression after syntax conversion. These are source and arithmetic checks, not native event or rendered-layout proof. Native build, mouse-wheel routing, final-row access, search/focus reveal, resize/category/tab transitions, languages and DPI combinations require the pinned runtime verification pass.

Wheel events reaching the embedded page are copied to the outer owner before the inner zero-range scroll helper can consume them. The copy retains the wheel delta, axis and modifiers and is delivered once. Reveal ownership accepts only an ancestor and uses a weak reference, so it cannot form a cycle or retain a destroyed host. Section changes settle the new content extents before resetting to the top and invalidating the sidebar for repaint.

The focused source/arithmetic checks now include wheel routing and transition ordering. These do not prove native event delivery. The integrated build must still verify wheel scrolling over page controls and empty space, focus/search reveal, final-row access and section switching at nonzero offsets.

When the embedded page background itself has focus, unmodified navigation keys are copied to the outer scroll owner. The inner keyboard scroll helper is disabled. Focused text fields and category controls keep their own key handling; modifiers and other keys are skipped normally.

The pre-repair native measurement recorded an embedded TabPrint allocation of 302 px with a natural header height of 152 px, while the following page began at y=308. Embedded headers now remeasure and constrain their height to their own sizer content during page fitting. This constraint does not run for standalone preset editors. Repeating the measurement in a configured profile without modal setup, and checking wrapped category rows after resize, remains required runtime verification.
