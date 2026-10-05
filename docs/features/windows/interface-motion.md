# Interface motion foundation

Preferences includes **Interface motion**, using the existing searchable native
selection control. **System** is the default and follows the operating system's
client-area animation preference. **Reduce motion** also requests immediate final
states from supported transitions. Neither choice forces animation against the
operating system's preference. The local `motion_preference` configuration key
stores `system` or `reduced`; missing or invalid values normalize to `system`.
The ordinary configuration save/load route preserves this choice across restarts.
English, Cantonese and bilingual presentation use the existing translation route.

`MD3::Motion::reduced()` reads the setting at each call. An active shared `Anim`
settles on its next timer delivery, normally 16 ms later, subject to UI event-loop
availability. There is no blocking wait or delayed input action. Existing switch,
camera and fan callers of this function also observe the saved preference.

New animation callers may supply a weak window owner. A hidden owner settles at
the final state without starting another timer; an owner destroyed during motion
cancels visual and completion callbacks at the next timer delivery. Its optional
`owner_lost` callback is only for resource cleanup, never for a user action.
`FadeIn` uses this binding and defers animator deletion on either completion or
owner loss. Existing callers without an owner retain their previous semantics.
Restart and destruction continue to invalidate the previous callback generation.

This is a foundation, not complete interface-wide motion. Panel switching, tooltip transitions, ImGui menu transitions and
notification/progress policy integration remain pending in
[`design/interface-motion.md`](../../../design/interface-motion.md).
No blanket hook animates every window, and child controls or OpenGL surfaces must
not acquire top-level layered-window fades implicitly.

## Verification

The hosted native-interface workflow compiles and executes 26 pure production
policy outcomes: system/explicit/unknown settings, operating-system precedence,
destroyed and hidden owners, and legacy ownerless behavior. These exercise the
actual policy functions, not a duplicate implementation. They do not verify timer
delivery, weak-reference invalidation, translation rendering or restart storage.

No local compilation, test or UI execution was performed for this change. Hosted
compilation and the policy result remain pending, followed by real preference
selection/restart and temporal evidence. Exercise interrupted/restarted animation,
owner destruction, hiding, preference changes during motion, focus/input and
stable final state. Use the exact installed executable hash and source revision.
Capture English, Cantonese and bilingual modes, both themes, normal and measured
minimum dimensions, and actual measured 100/125/150/200% scales. A still image
alone cannot establish a transition, its duration or an idle timer's absence.

## Native state colors and tab indicators

Button and StaticBox paint paths now request an interpolated color from their
StateHandler. Registered border, background and text colors transition for 100 ms;
semantic getters, enabled state, focus and input dispatch remain synchronous.
A reversal snapshots the currently painted color instead of jumping to the old
endpoint. Unconverted StateHandler consumers retain their existing immediate
painting and do not start an animation timer. Unregistered colors remain immediate.

Horizontal TabStrip activation moves the painted underline for 250 ms. Layout,
reordering, docking and resizing cancel that transition and use current geometry.
Vertical selection pills interpolate for 100 ms. Text weight, accessible selection,
focus rings and all hitboxes change immediately. Both routes use weak window owner
binding and stop their animator in destruction. Animation callbacks only assign
visual values and request paint; they do not dispatch semantic events or enter a
nested event loop. Shared generation checks protect restart and callback destruction.

Reduced motion paints the final colors/indicator immediately. The shared animator
settles hidden owners or a changed motion preference at its next event-loop delivery.
The existing interactive-card child-exit watchdog remains visible-only at 100 ms,
including reduced mode, because entering a child can suppress the parent's leave
event. It performs containment observation, not continuous decorative movement;
hidden or disabled cards stop it at the next delivery. Its reduced-mode colors snap.

The existing hosted policy executable now adds nine actual interpolation outcomes:
endpoint and out-of-range clamping, mid-flight reversal, bidirectional color rounding
and exact channel endpoints. The resulting 26 outcomes remain unexecuted for this
revision. They do not prove wx timer delivery, actual owner destruction, rendered
contrast during transition, child-exit behavior or tab geometry at runtime. Those
require the installed temporal and layout matrix described above. Panel content,
unconverted controls and the other inventory rows remain pending.