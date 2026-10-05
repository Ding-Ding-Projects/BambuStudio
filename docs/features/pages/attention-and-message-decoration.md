# Attention accommodations and message decoration

Focus, Low stimulation, Time awareness, One thing at a time and Momentum are independent per-visitor controls. All five default off. They are interface accommodations with no diagnosis, clinical claim, productivity score or inferred task.

Focus emphasizes the clicked or focused item while others remain readable, visible and usable. Clear focus restores equal emphasis. Low stimulation removes decorative motion, quiets vivid chrome and suppresses nonessential popups and narration, retaining notification history. Errors and warnings remain visible. Platform reduced-motion continues to apply. The eligible startup photo surprise is suppressed during quiet mode, without a separate surprise opt-out.

Time awareness shows session minutes and minutes since a visitor-state change beside the active page. It updates every 30 seconds without announcing every tick. One thing at a time retains a visitor-chosen next action across tab changes and reloads. Its edit button reaches the exact setting. Input is limited to 512 characters; invalid input leaves the previous value intact.

Momentum offers a factual dismissible reminder after 20 unchanged minutes. Not now pauses it for a full 30 minutes; settings exposes the same action. Low stimulation also suppresses these reminders. Changing one accommodation never toggles another.

Show emojis in dialogs and messages defaults on. A relevant decoration has `aria-hidden=true`. Factual copy, control labels, buttons and accessible names remain unchanged. Turning it off removes the decoration live and persists across reloads.

The existing settings search indexes these controls and has its own adjacent anchored regex builder. English, Cantonese and bilingual tone retain exact minutes and actions. Command-palette registration, append-only local history, shared School-mode integration and genuine browser captures remain unfinished requirements.

Verification: `node --test ui-md3/tests/site-attention.test.mjs ui-md3/tests/site.test.mjs ui-md3/tests/site-behaviour.test.mjs` passed 42 checks: 7 accommodation/emoji, 24 copy/site, 11 preferences/notifications. The hand-written five-mode inventory is deliberately broken one mode at a time, must fail, then passes on restoration. This is source-level evidence. Browser runtime proof remains unavailable until the approved hidden launch seam is repaired.

## Suggested articles

- [Event narration](event-narration.md)
- [Settings and appearance](settings-and-appearance.md)
- [Notifications](notifications.md)
- [Hidden browser verification](hidden-browser-verification.md)
