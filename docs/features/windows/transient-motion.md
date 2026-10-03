# Tooltip and dialog entrance motion

Native parameter tooltip cards, dialogs with `MD3DialogCaption`, and the command
palette own one entrance controller per surface. Their actual show event queues
the visual entrance until native visibility is available. Constructor-time chrome
setup only requests rounded corners. It no longer starts a fade while hidden.

The parameter card uses the shared short duration; dialogs and the command
palette use the shared medium duration. The controller uses the shared easing,
weak owner and reduced-motion preference. It never delays showing, focus, modal
state, close, acceptance or other semantic actions. Hide stops the timer,
restores opacity and invalidates a queued entrance. Reopening replaces the old
run. A visible parameter-card content update does not restart the entrance.

Only the explicitly supplied top-level Windows handle receives an alpha fade.
Child handles and already-layered surfaces are skipped. The controller restores
only the style it installed on its own unchanged handle; the parameter card's
separate shadow is not animated. Reduced motion leaves the final opaque state
immediately, and a preference change during a run settles on its next timer
delivery. Destruction invalidates the owner and cancels callbacks. Unsupported
platforms retain the ordinary immediate show behavior.

## Pending web tooltip coverage

`MarkdownTip` remains unanimated. Its `wxWebView` content can be attached to a
different parent, and its asynchronous loading and pending-script queue do not
currently carry an owner-scoped animation generation or the saved motion
preference. `resources/tooltip/styled.html` hosts the content container and the
bundled `showMarkdown` function replaces its HTML; there is no dedicated entrance
lifecycle handshake there. Native alpha on that compositor is not established
by source inspection. A follow-up must bind a content-only CSS transition to the
current owner, loaded document and show generation, propagate reduced motion,
and cancel it on hide, replacement, attachment and destruction. This is pending
coverage, not a completed transition or a reason to animate the child handle.

## Required hosted evidence

No local compilation, tests or UI execution were performed. The implementation
is source-only until the exact combined candidate is built and exercised on a
hosted Windows runner. Verify first show, rapid hide/reopen, visible tooltip
replacement, modal acceptance and cancellation, owner destruction, timer startup
failure, and preference changes during motion. Check that queued entrances never
revive hidden surfaces and that completion leaves no layered style or running
timer owned by this controller. Existing transparent surfaces must stay unchanged.

Use genuine temporal frames as well as final-state captures, with exact package
and source identities. Cover native parameter cards, one reusable captioned
dialog and the command palette in English, Cantonese and bilingual modes, light
and dark themes, normal and measured minimum dimensions, and actual measured
100%, 125%, 150% and 200% scales. Verify keyboard focus, pointer actions, tooltips,
shadow alignment, readable content and absence of clipping. A still image cannot
prove transition timing, cancellation or idle timer behavior. Web tooltip motion
and unsupported compositor cases remain explicitly unverified.
