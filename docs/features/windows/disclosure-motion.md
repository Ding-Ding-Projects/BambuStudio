# Disclosure state feedback

The Prepare Filament header and Monitor connection-details button retain
immediate expansion and collapse. Their existing content visibility, scrolling,
layout and hit targets do not animate. A decorative two-DIP vertical rail grows
from a short centered mark to a longer mark when expanded, using the shared
100 ms standard transition. Collapse reverses from the current painted extent.

## Ownership and behavior

`FilamentDisclosureHeader` in `Plater.cpp` paints inside the header's existing
left padding. Both the header-label click and background click update the rail
after changing visibility and layout. AMS synchronization also updates the rail
when it expands the filament list. The label and palette glyph keep their full
semantic colors throughout the transition.

`ConnectionDisclosureBanner` in `SideTools.cpp` paints in the warning banner's
existing left sizer margin. The separate `ScalableButton` chevron retains its
bitmap, hover, keyboard-focus painting and geometry unchanged. The banner's
`StaticBox::doRender` override adds the rail before existing button content.
The chevron changes immediately. The disclosure reads actual details-panel
visibility, and status updates that hide the details reset the chevron and rail
without replaying motion. Repeated unchanged status updates do not restart or
stop the transition. This also prevents a stale expanded flag after reconnect
from making the next disclosure click close an already hidden panel.

Each control owns one `MD3::Motion::Anim` and binds it to its window. Restarting
cancels the previous callback and starts at the current extent. A direct hide
stops and settles immediately. An ancestor hide settles through the shared
owner-visibility check on the next timer notification; it does not keep a hidden
polling timer alive. Destruction stops the member animation. Disabled controls
and the shared reduced-motion preference settle to the final extent, including
a preference change during a transition. Paint reads current semantic colors
and DPI, with no cached color or physical-pixel geometry.

These rails are supplementary state feedback. They do not dim semantic text,
animate a focus ring, fade child windows, delay a command or move a hit target.
There are no new labels, persisted settings, network operations or exports.
Existing translations and personal-vocabulary handling remain unchanged.

## Verification status

Source inspection only. No local compilation, tests or application execution
were performed. The shared animation event-loop fixture is separate evidence
and does not prove these controls' rendering or event delivery.

The next installed Windows verification must retain per-click images and a
bounded temporal sequence for:

- Prepare Filament label and background toggles, rapid reversal, AMS-triggered
  expansion, and switching away while a transition is active.
- Monitor connection warning, expand/collapse, repeated unchanged status,
  reconnect hiding details, and a new disconnect followed by one expansion.
- Reduced motion before interaction and during a transition, direct and
  ancestor hide, destruction, theme changes and DPI changes.
- Full-strength text, glyph and focus-ring contrast; rails inside existing
  padding without obscuring content; unchanged layout, scrolling and hitboxes.

Cover normal and minimum supported client sizes, English/Cantonese/bilingual,
light/dark and actual 100/125/150/200 percent scales. Monitor evidence requires
a genuine reachable disconnected-device state; an unavailable device fixture
must remain unverified. No physical transfer or printing is needed.

Related: [Motion verification](motion-verification.md),
[Notification motion](notification-motion.md),
[Interface motion](interface-motion.md).
