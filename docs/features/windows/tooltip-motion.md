# Canvas tooltip decorative motion

`ImGuiWrapper::tooltip` and `IMSlider::show_tooltip` keep their existing immediate
text, background, sizing, hover timing, and dismissal. A narrow accent rail grows
within the existing left padding for 100 ms. It does not alter text alpha, contrast
colors, tooltip opacity, input regions, or any command. The wrapper uses the
inverse-surface accent; the preview slider uses its existing theme's primary
accent. No generic popup fade is applied to these tooltips.

The wrapper owns bounded context-local timelines, with at most 128 entries.
Identity combines the invoking window/item and displayed content. Registered items
without an ImGui item ID also bind their observed item rectangle. Manual slider
hover regions instead pass an explicit slider, route and actual tick identity
through the tooltip renderer. This covers existing ticks, hovered layer time,
simulation play/pause and simulation speed, none of which register an item before
their tooltip. This prevents
the recycled internal tooltip window from sharing the preceding item's timeline.
Appearing windows, changed content, a frame gap, or reversed context time restart
the decoration. Context changes clear state, and entries not used in the current
frame are erased during rendering. Only a visible tooltip still within its
transition requests an extra canvas frame through the existing scheduler.

There is no timer, asynchronous callback, captured native owner, or stored
draw-list pointer. Destruction drops the wrapper-owned state. Hidden canvases do
not gain a timer. Reduced motion draws the final rail immediately and settles the
timeline so changing back does not replay a completed entrance. When the existing
enhanced-canvas scheduling feature is disabled, decoration is omitted rather
than adding another scheduler. A capacity or unusable-padding case retains the
complete original tooltip without decoration.

The preview route includes actual simulation play/pause and speed help, layer
information, pause/custom-code descriptions, and the existing layer-path toggle
help through `IMSlider::show_tooltip`. This slice does not add canvas menu hover
or filter animation and does not claim coverage of unrelated tooltip APIs.

## Verification still required

Source inspection is not rendered proof. Compile and execute only on hosted
Windows against the exact source/package. Observe first hover, rapid movement
between equal-label items, changing layer content, disappearance/reappearance,
canvas hide/show, context replacement, reduced motion before and during display,
and theme/DPI changes. Check the rail remains inside padding and never overlaps
text at English, Cantonese and bilingual normal/minimum tuples. Semantic text and
input must remain immediately available, without a delayed action or moving
hitbox. Record original per-action images and actual temporal request intervals.

The bounded temporal helper can potentially observe this paint-owned rail when
its actual four-frame intervals and region pixels qualify. Its requested sample
offsets alone are not evidence; missed cadence remains not observed. Human review
must distinguish the intended rail from other pixel changes. No local build,
test, application execution, or screenshot was performed for this source change.
