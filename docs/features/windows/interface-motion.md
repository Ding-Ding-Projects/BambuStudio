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

This is a foundation, not complete interface-wide motion. State colors, tab
indicators, panel switching, tooltip transitions, ImGui menu transitions and
notification/progress policy integration remain pending in
[`design/interface-motion.md`](../../../design/interface-motion.md).
No blanket hook animates every window, and child controls or OpenGL surfaces must
not acquire top-level layered-window fades implicitly.

## Verification

The hosted native-interface workflow compiles and executes 17 pure production
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
