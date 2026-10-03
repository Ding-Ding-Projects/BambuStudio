# Menu motion

Native material menus now own their entrance controller. Opening or reopening
schedules a 100 ms entrance after native visibility; dismissal and destruction
stop the controller and restore opacity before continuing normal teardown.
Existing submenu/search-builder dismissal exclusions remain unchanged. There is
no exit delay and no postponed command callback.

The native list interpolates its hover layer for 100 ms and refreshes changed
filter results with a fading decorative leading edge over 100 ms. Text, shortcuts
and essential glyph colors remain exact at every progress value. Filtering changes
the actual result list, selection and geometry immediately. Reversal starts at
the current hover weights. Keyboard selection remains immediately highlighted.
Reduced motion paints final state; hidden owners settle and destroyed owners
cancel through the shared weak-owner animator. The pre-existing submenu hover
delay remains a separate interaction rule, not an animation completion callback.
Empty-result text and bitmap icons remain immediate.

For canvas popups, ImGuiWrapper owns a bounded timeline keyed by context, window
ID and popup ID. The popup ID prevents a recycled depth window from inheriting a
different menu's entrance. Active visible popup draw lists receive a
100 ms alpha entrance, from 60% to full opacity. The implementation changes no
vertices' positions, clip rectangles, input rectangles, selection or lifetime.
It saves and restores the original vertex colors around the actual renderer,
including exception unwinding. At most 128 popup timelines, 1024 visited draw lists and 262144 vertex
colors are retained for a render; excess surfaces remain fully visible.

The checked-in ImGui API supplies Active, Hidden, Appearing, PopupId, FrameCount
and Time. The existing GLCanvas3D idle path consumes requires_extra_frame after
rendering. The entrance requests frames only while unsettled; closed popups lose
their timeline, and reduced motion immediately settles retained timelines.
Builds without ENABLE_ENHANCED_IMGUI_SLIDER_FLOAT retain immediate rendering
because their canvas does not consume that scheduling route. Canvas result-list changes and hover layers remain outside this entrance slice.
Modal popups, including the slider's Custom G-code and Jump to Layer dialogs and
the measurement distance popup, now share this route. Their modal input blocking remains immediate. The modal draw list also contains
its dim-background rectangle, so that paint participates in the entrance on top
of ImGui's existing DimBgRatio fade. Active child draw lists inherit the nearest
active visible popup ancestor's timeline through ParentWindow. A nested popup
owns a distinct timeline. The walk stops at ordinary non-child windows, hidden
ancestors or depth 64; it never fades arbitrary page content. A child appearing
later does not restart its owner's entrance. Each draw list is modified once.

RegexBuilderPopup now owns a transient entrance controller for its actual show
lifecycle. Show/hide observations are restricted to its own event object;
explicit Dismiss, native OnDismiss and destruction stop/restore the controller
before normal teardown. PopupAndFocusPattern retains its existing atomic focus
route, with no added owner activation or deferred pattern action. This applies
to both native SearchField and the canvas builder bridge.

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
hosted temporal and layout evidence. Actual native page-content composition
remains unresolved; decorative selection feedback is not a content crossfade.
Whole-interface motion is not complete.

## Canvas row and filter decoration

Canvas menu rows now retain their final text, shortcut, icon, background and focus
paint immediately. An independent 100 ms rail changes length inside spare left
padding. The shared custom selectable covers submenu headers and vertical menu
items. Explicit production hooks also cover canvas combo options, Assembly Export
rows and the filtered font list. A row without sufficient visible spare padding
skips decoration rather than covering content or widening the clip rectangle.
No click, dismissal, filtering, selection, layout or hitbox waits for animation.

One wrapper-owned map retains at most 128 decorations. Keys contain the nearest
popup window and popup identifier, current child window, actual submitted item
identifier and decoration kind. The owning ImGui context is checked before use.
Repeated submissions in a frame do not advance twice; unseen entries expire at
render. Hover reversal samples the current value before choosing the new target.
Saved or operating-system reduction settles immediately. Only visible active
interpolation requests an extra frame; no timer or asynchronous callback is added.

Search feedback uses the existing separator strip and a signature of the current
query plus accepted visible mask and unavailable state. Actual result changes can
restart that decoration; repeated worker polling with unchanged results cannot.
All result semantics and final content colors remain unchanged. The Assembly
Export hook saves its submitted item identity before a tooltip can replace the
last-item metadata.

These caller hooks and temporal behavior are source-only and require hosted
compilation and genuine intermediate captures. In particular, verify font-preview
padding, nested and child popup identities, live reduction, hover reversal,
asynchronous regex completion, hidden-item expiry and idle frame settlement.
No new executed test or rendered coverage is claimed by this change.

The hosted pure policy executable includes 33 additional outcomes using the actual
menu filter paint function, bringing its total to 59. They verify exact content
colors across four progress values and both reduction states, caption contrast
at least 4.5:1, and rejection of the former 60% blend below that threshold.
The existing 59 outcomes passed in hosted run `37101358501` at
`c7aefb4b84f31a4bc2519d8052ac78b3ccaa3a17`. They are source-color arithmetic,
not pixels, and do not verify the later canvas row/filter decoration added above.
