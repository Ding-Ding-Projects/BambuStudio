# Two-key destructive confirmation

Resetting this visitor's settings now opens an action-scoped confirmation. Its affected-data explanation is the originating action's localized copy. Key 1 acknowledges the affected data; Key 2 independently requests the exact action. The range slider remains disabled until both are operated, starts at 0, and authorizes only when it reaches exactly 100. Partial movement, one key, invalid input, cancellation or a repeated event cannot execute the action.

The panel anchors below or above the originating control where it fits, tracking scroll and resize. A constrained viewport uses a bounded modal fallback with internal scrolling and keyboard focus cycling. Emergency exit and Escape are available before authorization and restore focus to the originating control. The panel has its own local control search and adjacent anchored regex builder. Progress is factual and reduced-motion aware; completion identifies that the named action is running, not that its result has succeeded. An asynchronous failure says no success is claimed and requires a new reviewed prompt to retry.

Authorizations are isolated per prompt and consumed exactly once. The callback is never invoked before both keys and full slider completion. The shared component replaces the old one-button settings reset. Other destructive browser operations will adopt the same component as their real surfaces are added. The confirmation is part of this page, not an external service or detached helper.

Verification: `node --test ui-md3/tests/site-confirmation.test.mjs` covers seven source behavior cases, including untouched, one-key, both-key, partial/full slider, key withdrawal, invalid input, emergency cancellation, one-shot execution and prompt isolation. Negative regressions remove each independent-key check and observe an unauthorized run before restoration prevents it. Real browser geometry, keyboard and per-click captures remain unverified until the approved hidden route is repaired.

## Suggested articles

- [Settings and appearance](settings-and-appearance.md)
- [Notifications](notifications.md)
- [Hidden browser verification](hidden-browser-verification.md)
