# Prepare settings scrolling

The Prepare sidebar body owns vertical and horizontal scrolling for its embedded settings page. The page delegates focus and search-result reveal to that body and disables its own scroll ranges. Standalone preset editors keep a bounded page viewport and their own scrollbars.

Reveal treats each axis independently. A group wider than the viewport no longer suppresses vertical reveal; an oversized target exposes its leading edge. Sidebar relayout preserves the current pixel origin, clamps it through the scroll helper when content shrinks, and re-reads the client area after scrollbar strip reservation. A per-window scope prevents synchronous size-event recursion without suppressing another surface's layout.

The sidebar still permits horizontal access for settings rows whose minimum width exceeds the viewport. This change does not redesign those option rows.

Verification: the existing scrollbar tests pass 10 checks; the sidebar-width tests pass 4 checks; prepare-scroll-owner.test.mjs passes 4 checks. Removing reveal delegation deliberately fails the ownership check, and restoring it passes. The arithmetic test executes the production scalar expression after syntax conversion. These are source and arithmetic checks, not native event or rendered-layout proof. Native build, mouse-wheel routing, final-row access, search/focus reveal, resize/category/tab transitions, languages and DPI combinations require the pinned runtime verification pass.
