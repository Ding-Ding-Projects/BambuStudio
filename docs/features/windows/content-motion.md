# Content transition ownership and pending coverage

The content owners were inspected at source revision
`8a275071732f58b181e2e298eec493754d9c899a`. This slice implements bounded selection
feedback on the existing horizontal notebook underline and vertical tab button
marker. It does not implement page-content opacity or crossfade. Native transient
entrances and other tab indicators are documented separately; neither establishes
animated page content.

## Implemented selection feedback

`ButtonsListCtrl::SetSelection` and `TabButton::SetSelected` update selection
immediately, then emphasize the existing marker color from 65% to its full
semantic Primary color over the shared short duration and easing. The label,
selected page, focus, action dispatch and marker geometry do not wait or move.
The marker is never completely transparent. Its background and target colors
are resolved during painting, preserving the current theme and workspace scheme.

Each control owns one shared animator. A replacement selection stops the earlier
generation, deselection stops the vertical marker, and destruction stops the
timer. Weak-owner binding settles hidden controls and cancels destroyed owners.
Reduced motion paints the final marker immediately; an active timer observes the
preference on its next delivery. The implementation adds no native alpha, page
snapshot, overlay, compositor or hidden polling loop.

## Legacy Markdown tooltip

`MarkdownTip::ShowTip` returns `false` in `NDEBUG` builds. No external caller of
`ShowTip`, `AttachTo` or `DetachFrom` exists in the current `src` tree. The remaining
external calls are `Reload` in `PresetUpdater.cpp` and `Recreate`/`ExitTip` in
`MainFrame.cpp`; they do not create the singleton. Consequently, adding an
animation to that implementation would not animate a reachable release surface.
Its entrance is not applicable to the currently reachable release, rather than
being a completed animation.
The active native parameter tooltip has its own entrance controller instead.

If this legacy surface is deliberately restored in a future feature, its web
document must own content opacity rather than applying alpha to its child
handle. A safe implementation needs an exact current view and document identity,
load and show generations, saved and system reduced-motion state, and cancellation
on hide, replacement, attachment, reload and destruction. The current empty
`OnLoaded` callback, title-driven pending script and replaceable `_tipView` do not
provide that contract. The local `styled.html` container and bundled
`showMarkdown` content replacement are possible implementation points, not proof
of current reachability or a completed transition.

## Native page containers

`Notebook::DoShowPage` and `Tabbook::DoShowPage` call the page's
`ShowWithEffect`/`HideWithEffect` directly. Their default effects are
`wxSHOW_EFFECT_NONE` with zero timeout, and the current source has no caller
selecting a different effect. Selection, focus and page-change events therefore
retain their existing immediate behavior.

These containers do not own a composited image of their arbitrary child pages:

| Container owner | Actual content |
| --- | --- |
| `MainFrame` notebook | Home web content, device panels, project content and calibration |
| `MonitorPanel` tabbook | Status, storage, firmware and diagnostic panels |
| `CalibrationPanel` tabbook | Pressure advance and flow-rate wizard pages |
| `Auxiliary` tabbook | Project information, pictures, materials and assembly guide |
| `MultiMachinePage` tabbook | Device and local/cloud task managers |

There is no safe generic content-opacity implementation in these two container
classes alone. Applying layered alpha to arbitrary child or OpenGL handles is
not an accepted transition route. Blocking native show effects, delaying the
selected page, moving its live hitboxes or presenting a captured old page as
live content would alter the interaction contract.

The next implementation slice must select one reachable content owner with an
actual paint or web-compositor route. That owner can accept a generation-bound
visual transition while selection and focus change immediately. Native controls
need an owner-painted decoration or a real supported compositing layer; web
pages need their own loaded-document bridge; heavy canvas pages need renderer
support. Those are separate implementation scopes. The implemented selection
feedback adds no blanket page hook or speculative native effect.

## Verification still required

No local build, test, product or UI execution occurred. The marker changes still
require hosted compilation and real temporal evidence. Release reachability,
the chosen owner's rendering route, interrupted selection, hide/reopen,
destruction, reduced motion and focus/input stability must be verified against
an exact hosted package. Genuine temporal evidence is required in addition to
final-state captures. Exercise rapid selection replacement and removal during
motion, ancestor hide/show, theme and reduced-preference changes, keyboard focus,
marker contrast, and unchanged hitboxes at all required language, theme,
viewport and measured-scale tuples. Page-content transitions remain pending, including heavy
canvas and embedded web surfaces; an animated indicator is not their evidence.
