# Menu motion

Native material menus now own their entrance controller. Opening or reopening
schedules a 100 ms entrance after native visibility; dismissal and destruction
stop the controller and restore opacity before continuing normal teardown.
Existing submenu/search-builder dismissal exclusions remain unchanged. There is
no exit delay and no postponed command callback.

The native list interpolates its hover layer for 100 ms and refreshes changed
filter results from 60% to full text/icon opacity over 100 ms. Filtering changes
the actual result list, selection and geometry immediately. Reversal starts at
the current hover weights. Keyboard selection remains immediately highlighted.
Reduced motion paints final state; hidden owners settle and destroyed owners
cancel through the shared weak-owner animator. The pre-existing submenu hover
delay remains a separate interaction rule, not an animation completion callback.
Empty-result text and bitmap icons remain immediate.

For canvas popups, ImGuiWrapper owns a bounded timeline keyed by context, window
ID and popup ID. The popup ID prevents a recycled depth window from inheriting a
different menu's entrance. Active visible nonmodal popup draw lists receive a
100 ms alpha entrance, from 60% to full opacity. The implementation changes no
vertices' positions, clip rectangles, input rectangles, selection or lifetime.
It saves and restores the original vertex colors around the actual renderer,
including exception unwinding. At most 128 popup timelines and 262144 vertex
colors are retained for a render; excess surfaces remain fully visible.

The checked-in ImGui API supplies Active, Hidden, Appearing, PopupId, FrameCount
and Time. The existing GLCanvas3D idle path consumes requires_extra_frame after
rendering. The entrance requests frames only while unsettled; closed popups lose
their timeline, and reduced motion immediately settles retained timelines.
Builds without ENABLE_ENHANCED_IMGUI_SLIDER_FLOAT retain immediate rendering
because their canvas does not consume that scheduling route. Modal windows,
separate child draw lists, canvas result-list changes and hover layers remain
outside this canvas entrance slice. Native regex-builder dialogs use their
caption-owned transient entrance; this does not claim every builder uses it.

## Verification and remaining coverage

This is source implementation, not verified rendered behavior. No local build,
test or UI execution was performed. Hosted compilation and exact-package temporal
capture are pending. Verify root and nested menus, repeated open/dismiss, search
replacement, empty results, keyboard selection, rapid hover reversal, builder
focus ownership, destruction and reduced-motion changes during an entrance.
Confirm the idle frame request stops after settlement and no original vertex
colors remain modified after rendering. Real captures must bind source and
executable hashes, language, theme, measured scale and viewport. Cover English,
Cantonese and bilingual modes, light/dark themes, normal/minimum dimensions and
actual 100/125/150/200% scales, including intermediate text contrast and clipping.
A still image cannot establish timing, frame scheduling or lifetime behavior.

The target uses its checked-in native design references. Material Designer
creation/export tools were unavailable in this task, and local application
execution was prohibited. No prototype or source preview replaces the required
hosted temporal and layout evidence. Remaining canvas hover/filter and panel
transitions belong to subsequent bounded slices; whole-interface motion is not
complete.
